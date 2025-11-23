#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <memory>
#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <iostream>
#include <iomanip>
#include "aurora_intention.hpp"
#include "aurora_extreme.hpp"

namespace aurora {

// Classificazione biologica del flusso: tipo di tessuto di trasporto
enum class FlowClass : uint8_t {
    NERVE,   // latenza critica, payload piccolo, alta priorità
    MUSCLE,  // bulk data, throughput importante, può tollerare ritardi
    GLAND    // eventi rari, altissima affidabilità, meno sensibili alla latenza
};

// FASE 5b: Genotipo - strategia adattiva dell'organismo
enum class Genotype {
    BASELINE,        // comportamento attuale
    HYPERVIGILANT,   // reagisce forte ai fallimenti, molto conservativo
    STOIC,           // poco panico, stabile, pochi aggiustamenti
    EXPERIMENTAL     // più leggero, si permette di rischiare
};

// FASE 5b: Hint per la selezione del genotipo
enum class GenotypeHint {
    AUTO,
    FORCE_BASELINE,
    FORCE_HYPERVIGILANT,
    FORCE_STOIC,
    FORCE_EXPERIMENTAL
};

// Profilo logico del flusso: derivato da Intention, esteso con classificazione biologica
struct FlowProfile {
    double deadline_s;    // tempo massimo utile
    double reliability;   // 0..1
    double duty_limit;    // massimo duty RF/IR
    std::string priority; // "CRITICAL", "NORMAL", "BULK", ecc.
    FlowClass flow_class = FlowClass::MUSCLE;  // classificazione biologica
    GenotypeHint genotype_hint = GenotypeHint::AUTO;  // FASE 5b: hint per selezione genotipo
};

struct OrganismSpawnResult {
    std::vector<fec::Pkt> packets;   // simboli da seminare nella rete
    int K;                           // numero blocchi logici minimi
    std::size_t payload_size;        // dimensione totale payload in byte
};

struct OrganismIntegrateResult {
    bool delivered = false;                  // true se ricostruzione riuscita completa
    double coverage = 0.0;                   // 0..1, frazione di payload ricostruito in modo affidabile
    int symbols_used = 0;                    // numero simboli effettivamente usati per decode
    int total_symbols_seen = 0;              // numero totale simboli ricevuti per token_id
    std::vector<uint8_t> payload_bytes;      // payload ricostruito (parziale o completo)
};

// Interfaccia base per l'organismo di trasporto
class AuroraOrganism {
public:
    virtual ~AuroraOrganism() = default;

    // Costruisce un profilo flusso a partire da una Intention
    virtual FlowProfile build_profile(const Intention& I) const = 0;

    // Genera i simboli da seminare in rete a partire dal Token serializzato
    virtual OrganismSpawnResult spawn(
        const FlowProfile& profile,
        const std::string& token_id,
        const std::vector<uint8_t>& payload_bytes,
        size_t symbol_size = 128
    ) = 0;

    // Tenta di integrare i simboli arrivati per un token e ricostruire il payload
    virtual OrganismIntegrateResult integrate(
        const FlowProfile& profile,
        const std::string& token_id,
        int K_hint,
        size_t symbol_size,
        const std::vector<fec::Pkt>& received_packets
    ) = 0;
};

// Segmenti del payload: parte critica vs bulk
struct PayloadSegments {
    std::vector<uint8_t> critical;
    std::vector<uint8_t> bulk;
};

// Stato adattivo per ogni tipo di flusso: memoria immunitaria
struct FlowState {
    double crit_overhead = 1.0;        // overhead attuale parte critica
    double bulk_overhead = 1.0;        // overhead attuale parte bulk
    
    double base_crit_overhead = 1.0;   // overhead genetico base (parte critica)
    double base_bulk_overhead = 1.0;   // overhead genetico base (parte bulk)
    
    double avg_coverage = 0.0;         // EWMA della copertura
    
    int success_count = 0;             // conteggio successi
    int fail_count = 0;                // conteggio fallimenti
    
    int panic_boost = 0;               // cicli futuri in modalità "adrenalina"
    
    // FASE 3b: Storia minima per dimagrimento intelligente
    int good_streak = 0;               // numero di successi consecutivi "calmi" (senza panic)
    int bad_streak  = 0;               // numero di fallimenti consecutivi
    
    // FASE 5b: Genotipo e stato di inizializzazione
    Genotype genotype = Genotype::BASELINE;
    bool initialized = false;
    int age = 0;  // opzionale, per futuri usi/evoluzione
};

// Implementazione ALIENA avanzata: organo biologico con segmentazione e memoria immunitaria
class AlienFountainOrganism : public AuroraOrganism {
private:
    // Stato interno per ricordare K critico e bulk (utile per integrate)
    int _K_critical = 0;
    int _K_bulk = 0;
    size_t _critical_size = 0;
    size_t _bulk_size = 0;
    
    // Memoria immunitaria: stato adattivo per ogni tipo di flusso
    std::unordered_map<std::string, FlowState> flow_states_;
    
    // FASE 5b: Helper per selezione genotipo
    static Genotype choose_initial_genotype(const FlowProfile& profile) {
        switch (profile.flow_class) {
            case FlowClass::NERVE:  return Genotype::HYPERVIGILANT;
            case FlowClass::GLAND:  return Genotype::BASELINE;
            case FlowClass::MUSCLE: return Genotype::EXPERIMENTAL;
        }
        return Genotype::BASELINE;
    }
    
    static Genotype from_hint(GenotypeHint hint, const FlowProfile& profile) {
        switch (hint) {
            case GenotypeHint::FORCE_BASELINE:       return Genotype::BASELINE;
            case GenotypeHint::FORCE_HYPERVIGILANT: return Genotype::HYPERVIGILANT;
            case GenotypeHint::FORCE_STOIC:          return Genotype::STOIC;
            case GenotypeHint::FORCE_EXPERIMENTAL:   return Genotype::EXPERIMENTAL;
            case GenotypeHint::AUTO:                 return choose_initial_genotype(profile);
        }
        return Genotype::BASELINE;
    }
    
    static const char* genotype_to_string(Genotype g) {
        switch (g) {
            case Genotype::BASELINE:       return "BASELINE";
            case Genotype::HYPERVIGILANT:  return "HYPERVIGILANT";
            case Genotype::STOIC:          return "STOIC";
            case Genotype::EXPERIMENTAL:   return "EXPERIMENTAL";
        }
        return "UNKNOWN";
    }
    
    // FASE 5b: Parametri per genotipo
    struct GenoParams {
        double alpha_up;
        double alpha_down;
        double panic_multiplier;
        double max_overhead;
    };
    
    static GenoParams params_for(const FlowProfile& profile, const FlowState& st) {
        GenoParams gp{};
        // T3: Usa valori dalla config interattiva se disponibile
        auto& cfg = cl::get_interactive_config();
        double base_alpha_up = cfg.alpha_up;
        double base_alpha_down = cfg.alpha_down;
        
        switch (st.genotype) {
            case Genotype::BASELINE:
                gp.alpha_up = base_alpha_up;
                gp.alpha_down = base_alpha_down;
                gp.panic_multiplier = 1.0;
                gp.max_overhead = 4.0;
                break;
            case Genotype::HYPERVIGILANT:
                gp.alpha_up = base_alpha_up * 1.5;      // reagisce più forte ai fallimenti
                gp.alpha_down = base_alpha_down * 0.5;    // rilassa più lentamente
                gp.panic_multiplier = 2.0;
                gp.max_overhead = 6.0;
                break;
            case Genotype::STOIC:
                gp.alpha_up = base_alpha_up * 0.5;      // reagisce poco
                gp.alpha_down = base_alpha_down;
                gp.panic_multiplier = 0.5;
                gp.max_overhead = 3.5;
                break;
            case Genotype::EXPERIMENTAL:
                gp.alpha_up = base_alpha_up * 0.8;
                gp.alpha_down = base_alpha_down * 2.5;    // dimagrisce molto velocemente
                gp.panic_multiplier = 0.7;
                gp.max_overhead = 3.0;
                break;
        }
        return gp;
    }
    
    // Helper: costruisce chiave univoca per tipo di flusso
    static std::string make_flow_key(const FlowProfile& profile) {
        std::string cls;
        switch (profile.flow_class) {
            case FlowClass::NERVE:  cls = "NERVE"; break;
            case FlowClass::GLAND:  cls = "GLAND"; break;
            case FlowClass::MUSCLE: cls = "MUSCLE"; break;
        }
        return cls + ":" + profile.priority;
    }
    
public:
    FlowProfile build_profile(const Intention& I) const override {
        FlowProfile profile;
        profile.deadline_s = I.deadline_s;
        profile.reliability = I.reliability;
        profile.duty_limit = I.duty_frac;
        
        // Mappa reliability a priority
        if (profile.reliability >= 0.99) {
            profile.priority = "CRITICAL";
        } else if (profile.reliability >= 0.90) {
            profile.priority = "NORMAL";
        } else {
            profile.priority = "BULK";
        }
        
        // Classificazione biologica basata su deadline, reliability e payload atteso
        // NERVE: deadline molto bassa (< 2s) e priority alta (latency-critical)
        // GLAND: reliability > 0.95 e payload atteso grande (affidabilità estrema)
        // MUSCLE: tutto il resto (bulk data)
        if (profile.deadline_s < 2.0 && profile.reliability >= 0.90) {
            profile.flow_class = FlowClass::NERVE;
        } else if (profile.reliability > 0.95) {
            // Per ora assumiamo payload grande se reliability molto alta
            // In futuro potremmo usare un hint esplicito da Intention
            profile.flow_class = FlowClass::GLAND;
        } else {
            profile.flow_class = FlowClass::MUSCLE;
        }
        
        return profile;
    }
    
    // Helper: calcola overhead per parte critica in base a FlowClass
    static double crit_overhead_factor(const FlowProfile& p) {
        switch (p.flow_class) {
            case FlowClass::NERVE:
                return 3.0;  // massima ridondanza per latenza critica
            case FlowClass::GLAND:
                return 2.5;  // alta ridondanza per affidabilità estrema
            case FlowClass::MUSCLE:
            default:
                return 1.5;  // overhead moderato per bulk
        }
    }
    
    // Helper: calcola overhead per parte bulk in base a FlowClass
    static double bulk_overhead_factor(const FlowProfile& p) {
        switch (p.flow_class) {
            case FlowClass::NERVE:
                return 1.0;  // overhead minimo, focus su critico
            case FlowClass::GLAND:
                return 1.5;  // overhead moderato-alto per affidabilità
            case FlowClass::MUSCLE:
            default:
                return 1.2;  // overhead moderato per bulk
        }
    }
    
    // Helper: segmenta payload in critical e bulk
    PayloadSegments segment_payload(
        const std::vector<uint8_t>& payload_bytes,
        const FlowProfile& profile
    ) const {
        PayloadSegments segments;
        
        // Determina dimensione parte critica in base a FlowClass
        size_t critical_size;
        switch (profile.flow_class) {
            case FlowClass::NERVE:
                critical_size = std::min(static_cast<size_t>(256), payload_bytes.size());
                break;
            case FlowClass::GLAND:
                critical_size = std::min(static_cast<size_t>(512), payload_bytes.size());
                break;
            case FlowClass::MUSCLE:
            default:
                critical_size = std::min(static_cast<size_t>(128), payload_bytes.size());
                break;
        }
        
        // Estrai parte critica e bulk
        if (critical_size > 0 && critical_size < payload_bytes.size()) {
            segments.critical.assign(payload_bytes.begin(), payload_bytes.begin() + critical_size);
            segments.bulk.assign(payload_bytes.begin() + critical_size, payload_bytes.end());
        } else {
            // Payload troppo piccolo per segmentare, tutto in critico
            segments.critical = payload_bytes;
            segments.bulk.clear();
        }
        
        return segments;
    }

    OrganismSpawnResult spawn(
        const FlowProfile& profile,
        const std::string& token_id,
        const std::vector<uint8_t>& payload_bytes,
        size_t symbol_size = 128
    ) override {
        OrganismSpawnResult result;
        result.payload_size = payload_bytes.size();
        
        // Recupera/initializza stato adattivo per questo tipo di flusso
        auto key = make_flow_key(profile);
        auto& st = flow_states_[key];
        
        // FASE 5b: Inizializza genotipo se non ancora fatto
        if (!st.initialized) {
            st.genotype = from_hint(profile.genotype_hint, profile);
            st.initialized = true;
            st.age = 0;
            
            // Log una tantum quando il genotipo viene inizializzato
            std::string cls_name;
            switch (profile.flow_class) {
                case FlowClass::NERVE:  cls_name = "NERVE"; break;
                case FlowClass::GLAND:  cls_name = "GLAND"; break;
                case FlowClass::MUSCLE: cls_name = "MUSCLE"; break;
            }
            std::cout << "[ALIEN][GENO] class=" << cls_name
                      << " genotype=" << genotype_to_string(st.genotype) << std::endl;
        }
        
        // Prima volta: inizializza dai fattori statici esistenti
        if (st.base_crit_overhead == 1.0 && st.base_bulk_overhead == 1.0 &&
            st.success_count == 0 && st.fail_count == 0 && st.avg_coverage == 0.0) {
            // Prima volta: inizializza dai fattori statici esistenti
            st.base_crit_overhead = crit_overhead_factor(profile);
            st.base_bulk_overhead = bulk_overhead_factor(profile);
            st.crit_overhead = st.base_crit_overhead;
            st.bulk_overhead = st.base_bulk_overhead;
        }
        
        st.age++;  // Incrementa età del genotipo
        
        // Segmenta payload in critical e bulk
        PayloadSegments segments = segment_payload(payload_bytes, profile);
        _critical_size = segments.critical.size();
        _bulk_size = segments.bulk.size();
        
        // Usa due encoder separati per critical e bulk
        // Usiamo lo stesso symbol_size per entrambi per semplicità
        fec::Encoder enc_crit(segments.critical, symbol_size);
        fec::Encoder enc_bulk(segments.bulk, symbol_size);
        
        _K_critical = segments.critical.empty() ? 0 : enc_crit.N();
        _K_bulk = segments.bulk.empty() ? 0 : enc_bulk.N();
        
        // K totale = somma dei K dei due encoder
        result.K = _K_critical + _K_bulk;
        
        // Calcola overhead effettivi per QUESTO spawn, includendo panic_boost
        double crit_ov = st.crit_overhead;
        double bulk_ov = st.bulk_overhead;
        
        if (st.panic_boost > 0) {
            // "Rilascio di citochine" – risposta acuta
            crit_ov *= 2.0;   // super-ridondanza sulla parte critica
            bulk_ov *= 1.5;   // leggera inflazione anche sul bulk
            st.panic_boost -= 1;
        }
        
        int num_sym_crit = 0;
        int num_sym_bulk = 0;
        
        if (_K_critical > 0) {
            num_sym_crit = static_cast<int>(std::ceil(_K_critical * crit_ov));
            // Sicurezza minima: almeno K simboli
            if (num_sym_crit < _K_critical) num_sym_crit = _K_critical;
        }
        if (_K_bulk > 0) {
            num_sym_bulk = static_cast<int>(std::ceil(_K_bulk * bulk_ov));
            // Sicurezza minima: almeno K simboli
            if (num_sym_bulk < _K_bulk) num_sym_bulk = _K_bulk;
        }
        
        // Pre-alloca spazio per tutti i pacchetti
        result.packets.reserve(num_sym_crit + num_sym_bulk);
        
        // Genera simboli critici
        for (int i = 0; i < num_sym_crit; ++i) {
            fec::Fp fp = enc_crit.emit();
            result.packets.push_back({fp, 0, token_id, fec::SegmentKind::CRITICAL});
        }
        
        // Genera simboli bulk
        for (int i = 0; i < num_sym_bulk; ++i) {
            fec::Fp fp = enc_bulk.emit();
            result.packets.push_back({fp, 0, token_id, fec::SegmentKind::BULK});
        }
        
        return result;
    }

    OrganismIntegrateResult integrate(
        const FlowProfile& profile,
        const std::string& token_id,
        int K_hint,
        size_t symbol_size,
        const std::vector<fec::Pkt>& received_packets
    ) override {
        OrganismIntegrateResult result;
        result.delivered = false;
        result.coverage = 0.0;
        result.symbols_used = 0;
        result.total_symbols_seen = 0;
        
        // Filtra received_packets per token_id
        std::vector<fec::Pkt> filtered;
        filtered.reserve(received_packets.size());
        for (const auto& p : received_packets) {
            if (p.token_id == token_id) {
                filtered.push_back(p);
            }
        }
        
        result.total_symbols_seen = static_cast<int>(filtered.size());
        
        if (filtered.empty()) {
            return result;  // No packets for this token
        }
        
        // Separare pacchetti critici vs bulk
        std::vector<fec::Pkt> critical_packets;
        std::vector<fec::Pkt> bulk_packets;
        critical_packets.reserve(filtered.size());
        bulk_packets.reserve(filtered.size());
        
        for (const auto& p : filtered) {
            if (p.kind == fec::SegmentKind::CRITICAL) {
                critical_packets.push_back(p);
            } else {
                bulk_packets.push_back(p);
            }
        }
        
        // Usa K critico e bulk se disponibili, altrimenti stima da K_hint
        int K_crit = _K_critical > 0 ? _K_critical : (K_hint / 2);
        int K_bulk = _K_bulk > 0 ? _K_bulk : (K_hint - K_crit);
        
        // Stima dimensioni se non disponibili
        size_t expected_critical_size = _critical_size > 0 ? _critical_size : (K_crit * symbol_size);
        size_t expected_bulk_size = _bulk_size > 0 ? _bulk_size : (K_bulk * symbol_size);
        size_t expected_total_size = expected_critical_size + expected_bulk_size;
        
        // Usa due decoder separati
        bool crit_ok = false;
        bool bulk_ok = false;
        std::vector<uint8_t> bytes_crit;
        std::vector<uint8_t> bytes_bulk;
        int symbols_used_crit = 0;
        int symbols_used_bulk = 0;
        
        // Decode parte critica
        if (!critical_packets.empty() && K_crit > 0) {
            fec::Decoder dec_crit(K_crit, symbol_size);
            for (const auto& p : critical_packets) {
                dec_crit.push(p.fp);
                symbols_used_crit++;
            }
            auto [ok, bytes] = dec_crit.solve();
            if (ok && !bytes.empty()) {
                crit_ok = true;
                bytes_crit = std::move(bytes);
            }
        }
        
        // Decode parte bulk
        if (!bulk_packets.empty() && K_bulk > 0) {
            fec::Decoder dec_bulk(K_bulk, symbol_size);
            for (const auto& p : bulk_packets) {
                dec_bulk.push(p.fp);
                symbols_used_bulk++;
            }
            auto [ok, bytes] = dec_bulk.solve();
            if (ok && !bytes.empty()) {
                bulk_ok = true;
                bytes_bulk = std::move(bytes);
            }
        }
        
        result.symbols_used = symbols_used_crit + symbols_used_bulk;
        
        // Calcola coverage basata su parti ricostruite
        size_t covered_bytes = 0;
        if (crit_ok) {
            covered_bytes += bytes_crit.size();
        }
        if (bulk_ok) {
            covered_bytes += bytes_bulk.size();
        }
        
        if (expected_total_size > 0) {
            result.coverage = static_cast<double>(covered_bytes) / static_cast<double>(expected_total_size);
            result.coverage = std::clamp(result.coverage, 0.0, 1.0);
        }
        
        // Componi payload finale
        if (crit_ok || bulk_ok) {
            result.payload_bytes.reserve(bytes_crit.size() + bytes_bulk.size());
            if (crit_ok) {
                result.payload_bytes.insert(result.payload_bytes.end(), 
                                          bytes_crit.begin(), bytes_crit.end());
            }
            if (bulk_ok) {
                result.payload_bytes.insert(result.payload_bytes.end(), 
                                          bytes_bulk.begin(), bytes_bulk.end());
            }
        }
        
        // delivered = true solo se coverage completo (100%)
        // Per compatibilità con Engine, manteniamo questa logica conservativa
        result.delivered = (result.coverage >= 1.0);
        
        // Aggiorna stato adattivo basato sul risultato
        auto key = make_flow_key(profile);
        auto it = flow_states_.find(key);
        if (it != flow_states_.end()) {
            update_flow_state(profile, it->second, result.coverage, result.delivered,
                            result.symbols_used, result.total_symbols_seen);
        }
        
        return result;
    }
    
private:
    // Aggiorna stato adattivo: memoria immunitaria + panic mode
    void update_flow_state(
        const FlowProfile& profile,
        FlowState& st,
        double coverage,
        bool delivered,
        int symbols_used,
        int total_symbols_seen
    ) {
        // FASE 5b: Ottieni parametri basati sul genotipo
        auto gp = params_for(profile, st);
        const double MIN_OV = 1.0;
        const double max_overhead = gp.max_overhead;
        const double alpha_cov = 0.2;  // EWMA per coverage
        const double alpha_up = gp.alpha_up;  // reazione veloce al pericolo (dipende da genotipo)
        const double alpha_down = gp.alpha_down; // rilassamento lento (dipende da genotipo)
        
        // Aggiorna avg_coverage (EWMA)
        if (st.success_count + st.fail_count == 0) {
            st.avg_coverage = coverage;
        } else {
            st.avg_coverage = alpha_cov * coverage + (1.0 - alpha_cov) * st.avg_coverage;
        }
        
        // Aggiorna contatori
        if (delivered) {
            st.success_count++;
        } else {
            st.fail_count++;
        }
        
        // FASE 3b TASK 2: Aggiorna streak di successi/fallimenti (semplice, senza dipendere da panic)
        if (delivered) {
            st.good_streak += 1;
            st.bad_streak = 0;
        } else {
            st.bad_streak += 1;
            st.good_streak = 0;
        }
        
        // CASO 1: Fallimento → infezione / infiammazione
        // FASE 3b TASK 3: Raffinare la logica di up-adapt usando bad_streak
        // FASE 5b: Usa panic_multiplier dal genotipo
        if (!delivered) {
            // Aumento difese di base (moltiplicato per panic_multiplier)
            st.crit_overhead += alpha_up * gp.panic_multiplier;
            st.bulk_overhead += alpha_up * 0.5 * gp.panic_multiplier;
            
            // Panic Boost per flussi critici
            if (profile.flow_class == FlowClass::NERVE ||
                profile.flow_class == FlowClass::GLAND) {
                // T3: Usa panic_boost_steps dalla config
                auto& cfg = cl::get_interactive_config();
                st.panic_boost = std::max(st.panic_boost, cfg.panic_boost_steps); // cicli "adrenalinici" dalla config
                // Booster extra immediato (moltiplicato per panic_multiplier)
                st.crit_overhead += alpha_up * gp.panic_multiplier;   // doppio impatto sul critico
                
                // Se i fallimenti sono ripetuti, aumenta ancora un po' (shock prolungato)
                if (st.bad_streak >= 3) {
                    st.crit_overhead += alpha_up * 0.5 * gp.panic_multiplier;
                    st.bulk_overhead += alpha_up * 0.5 * gp.panic_multiplier;
                }
            }
        }
        
        // CASO 2: Consegna riuscita → possibilità di "dimagrire"
        if (delivered && total_symbols_seen > 0) {
            double efficiency = static_cast<double>(symbols_used) /
                               static_cast<double>(total_symbols_seen);
            
            // Se abbiamo usato pochi simboli rispetto a quelli inviati,
            // stavamo "sprecando" ridondanza: riduciamo lentamente.
            if (efficiency < 0.5) {
                st.crit_overhead = st.crit_overhead - alpha_down;
                st.bulk_overhead = st.bulk_overhead - alpha_down;
            }
        }
        
        // FASE 3b TASK 4: Dimagrimento lento basato sullo stato calmo
        // NON dipende da efficiency (che nel test è sempre 1.0)
        const int GOOD_STREAK_THRESHOLD = 4;      // almeno 4 successi "calmi" di fila
        const double COV_GOOD_THRESHOLD = 0.85;   // copertura media alta (abbassato da 0.90 per attivazione più realistica)
        
        if (delivered &&
            st.panic_boost == 0 &&                // nessun adrenalina attiva
            st.good_streak >= GOOD_STREAK_THRESHOLD &&
            st.avg_coverage >= COV_GOOD_THRESHOLD) {
            
            // riduciamo lentamente l'overhead verso i valori genetici
            double delta = alpha_down;
            
            // per i flussi MUSCLE possiamo dimagrire un po' più aggressivi
            if (profile.flow_class == FlowClass::MUSCLE) {
                delta *= 1.5; // ma senza esagerare
            }
            
            st.crit_overhead -= delta;
            st.bulk_overhead -= delta;
        }
        
        // Clamp agli estremi
        // Non scendiamo mai sotto il valore genetico di base
        st.crit_overhead = std::max(st.crit_overhead, st.base_crit_overhead);
        st.bulk_overhead = std::max(st.bulk_overhead, st.base_bulk_overhead);
        
        // Non saliamo oltre il massimo consentito (dipende dal genotipo)
        st.crit_overhead = std::clamp(st.crit_overhead, MIN_OV, max_overhead);
        st.bulk_overhead = std::clamp(st.bulk_overhead, MIN_OV, max_overhead);
        
        // Log adattivo
        std::string cls_name;
        switch (profile.flow_class) {
            case FlowClass::NERVE:  cls_name = "NERVE"; break;
            case FlowClass::GLAND:  cls_name = "GLAND"; break;
            case FlowClass::MUSCLE: cls_name = "MUSCLE"; break;
        }
        
        // FASE 3b TASK 6: Log esteso con streak
        // FASE 5b: Aggiungi genotipo al log
        std::cout << "[ALIEN][ADAPT] class=" << cls_name
                  << " priority=" << profile.priority
                  << " cov=" << std::fixed << std::setprecision(2) << coverage
                  << " avg_cov=" << st.avg_coverage
                  << " delivered=" << (delivered ? "true" : "false")
                  << " used=" << symbols_used
                  << "/" << total_symbols_seen
                  << " crit_ov=" << st.crit_overhead
                  << " bulk_ov=" << st.bulk_overhead
                  << " panic=" << st.panic_boost
                  << " gs=" << st.good_streak
                  << " bs=" << st.bad_streak
                  << " geno=" << genotype_to_string(st.genotype)
                  << std::endl;
    }
};

} // namespace aurora

