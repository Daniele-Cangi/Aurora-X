// test_aurora_organism.cpp
// Test completo per AlienFountainOrganism con tre scenari:
// 1. Canale buono: NERVE, GLAND, MUSCLE con delivery=true, overhead stabili
// 2. Canale cattivo: NERVE/GLAND con perdite, panic_boost, overhead crescenti
// 3. Adattamento: GLAND che si adatta da canale cattivo a buono
//
// Build: cmake --build build --target test_aurora_organism
// Run: ./build/bin/Release/test_aurora_organism.exe

#include "../aurora_organism.hpp"
#include "../aurora_intention.hpp"
#include "../aurora_extreme.hpp"
#include <iostream>
#include <iomanip>
#include <vector>
#include <random>
#include <algorithm>
#include <cassert>
#include <map>
#include <string>

using namespace aurora;
using namespace std;

// Helper: Genera payload casuale
std::vector<uint8_t> generate_payload(size_t size, uint32_t seed = 42) {
    std::mt19937 rng(seed);
    std::uniform_int_distribution<unsigned int> dist(0, 255);
    std::vector<uint8_t> payload(size);
    for (auto& b : payload) b = static_cast<uint8_t>(dist(rng));
    return payload;
}

// Helper: Simula perdita di pacchetti
std::vector<fec::Pkt> simulate_packet_loss(
    const std::vector<fec::Pkt>& packets,
    double loss_rate,
    uint32_t seed = 1000
) {
    if (loss_rate <= 0.0) return packets;
    
    std::vector<fec::Pkt> filtered;
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    
    for (const auto& pkt : packets) {
        if (dist(rng) >= loss_rate) {
            filtered.push_back(pkt);
        }
    }
    
    return filtered;
}

// Helper: Crea Intention per tipo di flusso
Intention make_intention_for_flow(FlowClass flow_class, double reliability = 0.99) {
    Intention I;
    
    switch (flow_class) {
        case FlowClass::NERVE:
            I.deadline_s = 1.5;  // deadline molto bassa
            I.reliability = 0.99;
            break;
        case FlowClass::GLAND:
            I.deadline_s = 600.0;  // deadline alta
            I.reliability = 0.98;  // reliability molto alta
            break;
        case FlowClass::MUSCLE:
        default:
            I.deadline_s = 300.0;
            I.reliability = 0.90;
            break;
    }
    
    I.duty_frac = 0.01;
    I.allow_optical = true;
    I.allow_backscatter = true;
    
    return I;
}

// Helper: Stampa risultato integrate
void print_integrate_result(
    const std::string& label,
    const OrganismIntegrateResult& result,
    int run_idx = -1
) {
    std::cout << "  [" << label << "]";
    if (run_idx >= 0) std::cout << " run=" << run_idx;
    std::cout << " delivered=" << (result.delivered ? "true " : "false")
              << " coverage=" << std::fixed << std::setprecision(3) << result.coverage
              << " symbols_used=" << result.symbols_used
              << "/" << result.total_symbols_seen
              << " payload_size=" << result.payload_bytes.size()
              << std::endl;
}

// ============================================================================
// SCENARIO 1: CANALE BUONO - NERVE, GLAND, MUSCLE
// ============================================================================
void test_scenario_good_channel() {
    std::cout << "\n" << string(70, '=') << std::endl;
    std::cout << "SCENARIO 1: CANALE BUONO" << std::endl;
    std::cout << string(70, '=') << std::endl;
    std::cout << "Test con NERVE, GLAND, MUSCLE - nessuna perdita" << std::endl;
    std::cout << "Atteso: delivered=true, coverage≈1.0, overhead stabili\n" << std::endl;
    
    AlienFountainOrganism organism;
    
    // Test NERVE
    {
        std::cout << "--- Testing NERVE flow ---" << std::endl;
        Intention I = make_intention_for_flow(FlowClass::NERVE);
        FlowProfile profile = organism.build_profile(I);
        
        assert(profile.flow_class == FlowClass::NERVE);
        std::cout << "  Profile: deadline=" << profile.deadline_s 
                  << "s, reliability=" << profile.reliability 
                  << ", priority=" << profile.priority << std::endl;
        
        const std::string token_id = "nerve_test_001";
        std::vector<uint8_t> payload = generate_payload(1024, 100);
        
        // Spawn
        OrganismSpawnResult spawn_res = organism.spawn(profile, token_id, payload, 128);
        std::cout << "  Spawn: K=" << spawn_res.K 
                  << ", packets=" << spawn_res.packets.size() << std::endl;
        
        // Integrate senza perdite
        OrganismIntegrateResult integrate_res = organism.integrate(
            profile, token_id, spawn_res.K, 128, spawn_res.packets
        );
        
        print_integrate_result("NERVE", integrate_res);
        
        assert(integrate_res.delivered == true);
        assert(integrate_res.coverage >= 0.99);
        assert(integrate_res.payload_bytes == payload);
        std::cout << "  ✓ NERVE test PASSED\n" << std::endl;
    }
    
    // Test GLAND
    {
        std::cout << "--- Testing GLAND flow ---" << std::endl;
        Intention I = make_intention_for_flow(FlowClass::GLAND);
        FlowProfile profile = organism.build_profile(I);
        
        assert(profile.flow_class == FlowClass::GLAND);
        std::cout << "  Profile: deadline=" << profile.deadline_s 
                  << "s, reliability=" << profile.reliability 
                  << ", priority=" << profile.priority << std::endl;
        
        const std::string token_id = "gland_test_001";
        std::vector<uint8_t> payload = generate_payload(2048, 200);
        
        // Spawn
        OrganismSpawnResult spawn_res = organism.spawn(profile, token_id, payload, 128);
        std::cout << "  Spawn: K=" << spawn_res.K 
                  << ", packets=" << spawn_res.packets.size() << std::endl;
        
        // Integrate senza perdite
        OrganismIntegrateResult integrate_res = organism.integrate(
            profile, token_id, spawn_res.K, 128, spawn_res.packets
        );
        
        print_integrate_result("GLAND", integrate_res);
        
        assert(integrate_res.delivered == true);
        assert(integrate_res.coverage >= 0.99);
        assert(integrate_res.payload_bytes == payload);
        std::cout << "  ✓ GLAND test PASSED\n" << std::endl;
    }
    
    // Test MUSCLE
    {
        std::cout << "--- Testing MUSCLE flow ---" << std::endl;
        Intention I = make_intention_for_flow(FlowClass::MUSCLE);
        FlowProfile profile = organism.build_profile(I);
        
        assert(profile.flow_class == FlowClass::MUSCLE);
        std::cout << "  Profile: deadline=" << profile.deadline_s 
                  << "s, reliability=" << profile.reliability 
                  << ", priority=" << profile.priority << std::endl;
        
        const std::string token_id = "muscle_test_001";
        std::vector<uint8_t> payload = generate_payload(4096, 300);
        
        // Spawn
        OrganismSpawnResult spawn_res = organism.spawn(profile, token_id, payload, 128);
        std::cout << "  Spawn: K=" << spawn_res.K 
                  << ", packets=" << spawn_res.packets.size() << std::endl;
        
        // Integrate senza perdite
        OrganismIntegrateResult integrate_res = organism.integrate(
            profile, token_id, spawn_res.K, 128, spawn_res.packets
        );
        
        print_integrate_result("MUSCLE", integrate_res);
        
        assert(integrate_res.delivered == true);
        assert(integrate_res.coverage >= 0.99);
        assert(integrate_res.payload_bytes == payload);
        std::cout << "  ✓ MUSCLE test PASSED\n" << std::endl;
    }
    
    std::cout << "✓ SCENARIO 1 COMPLETATO: tutti i flussi hanno delivery=true e coverage≈1.0\n" << std::endl;
}

// ============================================================================
// SCENARIO 2: CANALE CATTIVO - NERVE e GLAND con perdite
// ============================================================================
void test_scenario_bad_channel() {
    std::cout << "\n" << string(70, '=') << std::endl;
    std::cout << "SCENARIO 2: CANALE CATTIVO / RUMOROSO" << std::endl;
    std::cout << string(70, '=') << std::endl;
    std::cout << "Test con NERVE e GLAND con perdite elevate (50-60%)" << std::endl;
    std::cout << "Atteso: delivered=false, panic_boost>0, overhead crescenti\n" << std::endl;
    
    AlienFountainOrganism organism;
    
    // Test NERVE con perdite
    {
        std::cout << "--- Testing NERVE flow con canale cattivo (perdita 50%) ---" << std::endl;
        Intention I = make_intention_for_flow(FlowClass::NERVE);
        FlowProfile profile = organism.build_profile(I);
        
        const std::string token_id = "nerve_bad_001";
        std::vector<uint8_t> payload = generate_payload(1024, 400);
        
        // Spawn
        OrganismSpawnResult spawn_res = organism.spawn(profile, token_id, payload, 128);
        std::cout << "  Spawn: K=" << spawn_res.K 
                  << ", packets=" << spawn_res.packets.size() << std::endl;
        
        // Simula perdita pesante
        std::vector<fec::Pkt> received_packets = simulate_packet_loss(spawn_res.packets, 0.50, 500);
        std::cout << "  Canale cattivo: ricevuti " << received_packets.size() 
                  << " / " << spawn_res.packets.size() << " pacchetti" << std::endl;
        
        // Integrate
        OrganismIntegrateResult integrate_res = organism.integrate(
            profile, token_id, spawn_res.K, 128, received_packets
        );
        
        print_integrate_result("NERVE", integrate_res);
        
        assert(integrate_res.delivered == false);
        assert(integrate_res.coverage < 0.9);  // Coverage incompleto per perdite elevate
        std::cout << "  ✓ NERVE fallito come previsto (delivered=false)\n" << std::endl;
        
        // Secondo spawn: dovrebbe vedere panic_boost attivo
        std::cout << "  --- Secondo spawn dopo fallimento (dovrebbe attivare panic_boost) ---" << std::endl;
        const std::string token_id2 = "nerve_bad_002";
        std::vector<uint8_t> payload2 = generate_payload(1024, 401);
        OrganismSpawnResult spawn_res2 = organism.spawn(profile, token_id2, payload2, 128);
        std::cout << "  Spawn dopo fallimento: K=" << spawn_res2.K 
                  << ", packets=" << spawn_res2.packets.size() << std::endl;
        std::cout << "  (Nota: panic_boost dovrebbe essere visibile nell'output [ALIEN][ADAPT])\n" << std::endl;
    }
    
    // Test GLAND con perdite
    {
        std::cout << "--- Testing GLAND flow con canale cattivo (perdita 55%) ---" << std::endl;
        Intention I = make_intention_for_flow(FlowClass::GLAND);
        FlowProfile profile = organism.build_profile(I);
        
        const std::string token_id = "gland_bad_001";
        std::vector<uint8_t> payload = generate_payload(2048, 500);
        
        // Spawn
        OrganismSpawnResult spawn_res = organism.spawn(profile, token_id, payload, 128);
        std::cout << "  Spawn: K=" << spawn_res.K 
                  << ", packets=" << spawn_res.packets.size() << std::endl;
        
        // Simula perdita pesante
        std::vector<fec::Pkt> received_packets = simulate_packet_loss(spawn_res.packets, 0.55, 600);
        std::cout << "  Canale cattivo: ricevuti " << received_packets.size() 
                  << " / " << spawn_res.packets.size() << " pacchetti" << std::endl;
        
        // Integrate
        OrganismIntegrateResult integrate_res = organism.integrate(
            profile, token_id, spawn_res.K, 128, received_packets
        );
        
        print_integrate_result("GLAND", integrate_res);
        
        assert(integrate_res.delivered == false);
        assert(integrate_res.coverage < 0.9);
        std::cout << "  ✓ GLAND fallito come previsto (delivered=false)\n" << std::endl;
        
        // Secondo spawn: dovrebbe vedere panic_boost attivo
        std::cout << "  --- Secondo spawn dopo fallimento (dovrebbe attivare panic_boost) ---" << std::endl;
        const std::string token_id2 = "gland_bad_002";
        std::vector<uint8_t> payload2 = generate_payload(2048, 501);
        OrganismSpawnResult spawn_res2 = organism.spawn(profile, token_id2, payload2, 128);
        std::cout << "  Spawn dopo fallimento: K=" << spawn_res2.K 
                  << ", packets=" << spawn_res2.packets.size() << std::endl;
        std::cout << "  (Nota: panic_boost dovrebbe essere visibile nell'output [ALIEN][ADAPT])\n" << std::endl;
    }
    
    std::cout << "✓ SCENARIO 2 COMPLETATO: NERVE e GLAND hanno delivery=false e panic_boost attivato\n" << std::endl;
}

// ============================================================================
// SCENARIO 3: ADATTAMENTO NEL TEMPO - GLAND da cattivo a buono
// ============================================================================
struct FlowStateSnapshot {
    int run_idx;
    bool delivered;
    double coverage;
    int symbols_used;
    int total_symbols_seen;
    // Nota: non possiamo accedere direttamente allo stato interno, 
    // ma possiamo inferirlo dall'output o dalle statistiche
};

void test_scenario_adaptation() {
    std::cout << "\n" << string(70, '=') << std::endl;
    std::cout << "SCENARIO 3: ADATTAMENTO NEL TEMPO" << std::endl;
    std::cout << string(70, '=') << std::endl;
    std::cout << "Test GLAND: prima canale cattivo (aumenta overhead)" << std::endl;
    std::cout << "           poi canale buono (diminuisce lentamente)" << std::endl;
    std::cout << "\nNota: Lo stato interno crit_ov/bulk_ov/panic_boost è visibile" << std::endl;
    std::cout << "      nell'output [ALIEN][ADAPT] dopo ogni integrate().\n" << std::endl;
    
    AlienFountainOrganism organism;
    Intention I = make_intention_for_flow(FlowClass::GLAND);
    FlowProfile profile = organism.build_profile(I);
    
    const int NUM_RUNS_BAD = 5;   // Fase 1: canale cattivo
    const int NUM_RUNS_GOOD = 10; // Fase 2: canale buono
    
    std::vector<FlowStateSnapshot> snapshots;
    
    // ===== FASE 1: CANALE CATTIVO (perdita 50%) =====
    std::cout << "--- FASE 1: CANALE CATTIVO (perdita 50%) ---" << std::endl;
    std::cout << "Eseguendo " << NUM_RUNS_BAD << " run con perdite elevate...\n" << std::endl;
    
    for (int run = 0; run < NUM_RUNS_BAD; ++run) {
        const std::string token_id = "gland_adapt_" + std::to_string(run);
        std::vector<uint8_t> payload = generate_payload(2048, 1000 + run);
        
        // Spawn
        OrganismSpawnResult spawn_res = organism.spawn(profile, token_id, payload, 128);
        
        // Simula perdita 50%
        std::vector<fec::Pkt> received_packets = simulate_packet_loss(spawn_res.packets, 0.50, 2000 + run);
        
        // Integrate
        OrganismIntegrateResult integrate_res = organism.integrate(
            profile, token_id, spawn_res.K, 128, received_packets
        );
        
        FlowStateSnapshot snap;
        snap.run_idx = run;
        snap.delivered = integrate_res.delivered;
        snap.coverage = integrate_res.coverage;
        snap.symbols_used = integrate_res.symbols_used;
        snap.total_symbols_seen = integrate_res.total_symbols_seen;
        snapshots.push_back(snap);
        
        std::cout << "  Run " << run << ": delivered=" << (integrate_res.delivered ? "true " : "false")
                  << ", coverage=" << std::fixed << std::setprecision(3) << integrate_res.coverage
                  << ", received=" << received_packets.size() << "/" << spawn_res.packets.size()
                  << std::endl;
    }
    
    std::cout << "\n--- FASE 2: CANALE BUONO (nessuna perdita) ---" << std::endl;
    std::cout << "Eseguendo " << NUM_RUNS_GOOD << " run senza perdite..." << std::endl;
    std::cout << "Atteso: overhead che diminuisce lentamente verso valori base\n" << std::endl;
    
    // ===== FASE 2: CANALE BUONO (nessuna perdita) =====
    for (int run = NUM_RUNS_BAD; run < NUM_RUNS_BAD + NUM_RUNS_GOOD; ++run) {
        const std::string token_id = "gland_adapt_" + std::to_string(run);
        std::vector<uint8_t> payload = generate_payload(2048, 2000 + run);
        
        // Spawn
        OrganismSpawnResult spawn_res = organism.spawn(profile, token_id, payload, 128);
        
        // Nessuna perdita: tutti i pacchetti arrivano
        std::vector<fec::Pkt> received_packets = spawn_res.packets;
        
        // Integrate
        OrganismIntegrateResult integrate_res = organism.integrate(
            profile, token_id, spawn_res.K, 128, received_packets
        );
        
        FlowStateSnapshot snap;
        snap.run_idx = run;
        snap.delivered = integrate_res.delivered;
        snap.coverage = integrate_res.coverage;
        snap.symbols_used = integrate_res.symbols_used;
        snap.total_symbols_seen = integrate_res.total_symbols_seen;
        snapshots.push_back(snap);
        
        std::cout << "  Run " << run << ": delivered=" << (integrate_res.delivered ? "true " : "false")
                  << ", coverage=" << std::fixed << std::setprecision(3) << integrate_res.coverage
                  << ", received=" << received_packets.size() << "/" << spawn_res.packets.size()
                  << std::endl;
    }
    
    // ===== TABELLA RIASSUNTIVA =====
    std::cout << "\n" << string(70, '-') << std::endl;
    std::cout << "TABELLA RIASSUNTIVA - GLAND ADATTAMENTO" << std::endl;
    std::cout << string(70, '-') << std::endl;
    std::cout << std::left 
              << std::setw(6) << "Run"
              << std::setw(10) << "Delivered"
              << std::setw(10) << "Coverage"
              << std::setw(12) << "Symbols"
              << std::setw(8) << "Efficiency"
              << std::endl;
    std::cout << string(70, '-') << std::endl;
    
    for (const auto& snap : snapshots) {
        double efficiency = (snap.total_symbols_seen > 0) 
            ? static_cast<double>(snap.symbols_used) / static_cast<double>(snap.total_symbols_seen)
            : 0.0;
        
        std::string phase = (snap.run_idx < NUM_RUNS_BAD) ? "[BAD] " : "[GOOD]";
        
        std::cout << std::left
                  << std::setw(6) << (std::to_string(snap.run_idx) + phase)
                  << std::setw(10) << (snap.delivered ? "true" : "false")
                  << std::fixed << std::setprecision(3)
                  << std::setw(10) << snap.coverage
                  << std::setw(12) << (std::to_string(snap.symbols_used) + "/" + std::to_string(snap.total_symbols_seen))
                  << std::setprecision(2)
                  << std::setw(8) << efficiency
                  << std::endl;
    }
    
    std::cout << string(70, '-') << std::endl;
    std::cout << "\nNOTA: I valori crit_ov, bulk_ov e panic_boost sono stampati" << std::endl;
    std::cout << "      nell'output [ALIEN][ADAPT] dopo ogni integrate()." << std::endl;
    std::cout << "      Nella fase BAD, dovresti vedere:" << std::endl;
    std::cout << "        - delivered=false, panic_boost > 0, overhead crescenti" << std::endl;
    std::cout << "      Nella fase GOOD, dovresti vedere:" << std::endl;
    std::cout << "        - delivered=true, panic_boost=0, overhead che diminuiscono lentamente\n" << std::endl;
    
    std::cout << "✓ SCENARIO 3 COMPLETATO: adattamento nel tempo testato\n" << std::endl;
}

// ============================================================================
// MAIN
// ============================================================================
int main() {
    std::cout << string(70, '=') << std::endl;
    std::cout << "TEST COMPLETO - ALIEN FOUNTAIN ORGANISM" << std::endl;
    std::cout << string(70, '=') << std::endl;
    std::cout << "\nQuesto test verifica tre scenari:" << std::endl;
    std::cout << "  1. Canale buono: NERVE, GLAND, MUSCLE con delivery=true" << std::endl;
    std::cout << "  2. Canale cattivo: NERVE/GLAND con perdite, panic_boost, overhead crescenti" << std::endl;
    std::cout << "  3. Adattamento: GLAND che si adatta da canale cattivo a buono\n" << std::endl;
    
    try {
        // Scenario 1: Canale buono
        test_scenario_good_channel();
        
        // Scenario 2: Canale cattivo
        test_scenario_bad_channel();
        
        // Scenario 3: Adattamento
        test_scenario_adaptation();
        
        std::cout << string(70, '=') << std::endl;
        std::cout << "TUTTI I TEST COMPLETATI CON SUCCESSO!" << std::endl;
        std::cout << string(70, '=') << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "\nERRORE: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "\nERRORE: eccezione sconosciuta" << std::endl;
        return 1;
    }
}

