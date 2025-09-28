// aurora_x.cpp
// AURORA-X — Extreme Field Orchestrator (3-file repo)
// Build Dev:   g++ -std=c++20 -O3 -pthread aurora_x.cpp -o aurora_x
// Build Field: g++ -std=c++20 -O3 -pthread -DFIELD_BUILD -DAURORA_USE_REAL_CRYPTO -DAURORA_USE_RAPTORQ_REAL aurora_x.cpp -lsodium -o aurora_x_field

#include <iostream>
#include <vector>
#include <string>
#include <array>
#include <memory>
#include <unordered_set>
#include <cmath>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <map>
#include <thread>
#include <atomic>
#include <chrono>
#include <thread>
#include <atomic>
#include <chrono>
using namespace std; using namespace chrono;

#include "aurora_intention.hpp"
#include "aurora_hal.hpp"
#include "aurora_extreme.hpp"



// ---------- Token/Bundle ----------
struct Token {
  string id, payload; uint64_t created=0, expiry=0;
  array<uint8_t,32> pk{}; array<uint8_t,64> sk{}; array<uint8_t,64> sig{};
  static Token make(const string& payload, uint64_t ttl){
    Token t; CRYPTO::ed25519_keypair(t.pk.data(), t.sk.data());
    t.id = util::h64(payload + to_string(ttl) + to_string(util::rng.next()));
    t.payload = payload; t.created = util::now_s(); t.expiry = t.created + ttl;
    string msg = t.id + t.payload + to_string(t.expiry);
    CRYPTO::ed25519_sign(t.sig.data(), (const uint8_t*)msg.data(), msg.size(), t.sk.data());
    return t;
  }
  bool verify() const {
    string msg = id + payload + to_string(expiry);
    return CRYPTO::ed25519_verify(sig.data(), (const uint8_t*)msg.data(), msg.size(), pk.data());
  }
};
struct Bundle { string bid; Token tok; uint64_t expiry; bool custody=true;
  static Bundle make(const Token& t){ return { util::h64("B"+t.id), t, t.expiry, true }; }
};
static vector<uint8_t> tok2bytes(const Token& t){
  vector<uint8_t> o; auto putS=[&](const string& s){ uint32_t L=s.size(); for(int i=0;i<4;++i) o.push_back((L>>(8*i))&0xFF); o.insert(o.end(), s.begin(), s.end()); };
  putS(t.id); putS(t.payload); for(int i=0;i<8;++i) o.push_back((t.created>>(8*i))&0xFF); for(int i=0;i<8;++i) o.push_back((t.expiry>>(8*i))&0xFF);
  o.insert(o.end(), t.pk.begin(), t.pk.end()); o.insert(o.end(), t.sig.begin(), t.sig.end()); return o;
}
static Token bytes2tok(const vector<uint8_t>& v){
  size_t off=0; auto getS=[&](string& s){ uint32_t L=0; for(int i=0;i<4;++i) L|=((uint32_t)v[off+i])<<(8*i); off+=4; s.assign((const char*)&v[off], (const char*)&v[off+L]); off+=L;};
  Token t; getS(t.id); getS(t.payload);
  t.created=0; for(int i=0;i<8;++i) t.created|=((uint64_t)v[off+i])<<(8*i); off+=8;
  t.expiry=0;  for(int i=0;i<8;++i) t.expiry |=((uint64_t)v[off+i])<<(8*i); off+=8;
  memcpy(t.pk.data(), &v[off], 32); off+=32; memcpy(t.sig.data(), &v[off], 64); off+=64; return t;
}

// ---------- Network state ----------
struct Node {
  string id; geom::Vec2 pos;
  energy::Store bat{10.0, 6.0};
  vector<fec::Pkt> buf, inbox; unordered_set<uint32_t> seen;
  double harvest_W = 0.2;
  void tick(double dt){ bat.harvest(harvest_W, dt); }
  void ingest(){ for(auto&p: inbox) if(!seen.count(p.seq)){ buf.push_back(p); seen.insert(p.seq);} inbox.clear(); }
  bool send_one(world::World& W, Node& rx, phy::Mode m){
    if(buf.empty()) return false; auto pkt = buf.back();
    size_t B = pkt.fp.data.size()+8;
    double J = phy::Jpkt(m,B); if(!bat.spend(J)) return false;
    double g  = W.multibounce_best(pos, rx.pos, 2);
    double SNR= phy::snr_db(g,m,W.illum);
    double coding_gain = (m==phy::Mode::RF? 8.0 : m==phy::Mode::IR? 3.0 : 4.0);
    bool ok = channel::pass_realistic(SNR, m, coding_gain, /*rician*/ false);
    if(ok && !rx.seen.count(pkt.seq)) rx.inbox.push_back(pkt);

    // size shaping safe — padding solo nel frame PHY (non parte del FEC/Merkle)
    if(m==phy::Mode::RF){
      int pad = (int)(util::rng.uni()*12);
      vector<uint8_t> frame = pkt.fp.data; frame.resize(frame.size()+pad, (uint8_t)(0xA5 ^ pad));
      HAL::LORA_TX(frame.data(), frame.size());
    } else if(m==phy::Mode::IR){
      HAL::IR_TX(pkt.fp.data.data(), pkt.fp.data.size(), 3500 + (int)(util::rng.uni()*1200));
    } else {
      vector<uint8_t> bits( pkt.fp.data.size()*8, 1 );
      HAL::BS_MODULATE(bits.data(), bits.size(), 450 + (int)(util::rng.uni()*200));
    }
    return ok;
  }
};

struct Net { world::World W; vector<unique_ptr<Node>> nodes;
  Node& add(const string& id, geom::Vec2 p){ nodes.emplace_back(make_unique<Node>()); nodes.back()->id=id; nodes.back()->pos=p; return *nodes.back(); }
  Node* get(const string& id){ for(auto& n:nodes) if(n->id==id) return n.get(); return nullptr; }
};

// ---------- Engine ----------
struct Engine {
  Net net; Intention I; string token_id, bundle_id; int K; size_t T=256; uint32_t seqc=1;

  void init(const string& intention){
    I = Intention::parse(intention);
    net.W.obs.push_back({{0.52,0.52}, 0.22});
    for(int i=0;i<I.ris_tiles; ++i){ double u=(i+1.0)/(I.ris_tiles+1.0); net.W.ris.push_back({{0.12+0.78*u, 0.12+0.78*u}, (uint8_t)(i%4)}); }
    HAL::RADIO_INIT();
    HAL::LORA_CFG(phy::EU868_CH[0], 125, 12, 5, 12);
    net.add("SRC",{0.06,0.08}); net.add("DST",{0.94,0.92});

    Token t = Token::make("ACCESS:TEMP_KEY=abc123;ZONE=42;TTL=24h;CLASS=NORM;", 24*3600);
    Bundle b = Bundle::make(t); token_id=t.id; bundle_id=b.bid;
    auto bytes = tok2bytes(t); fec::Encoder enc(bytes, T); K = enc.N();
    for(int i=0;i<K*3; ++i){ auto fp = enc.emit(); net.get("SRC")->buf.push_back({fp, seqc++, token_id}); }
  }

  static double entropy_residual(int have, int need){ double e=max(0, need-have)/(double)need; return min(1.0,max(0.0,e)); }

  bool run(){
    using HAL::FHSS_next;
    Node& S=*net.get("SRC"); Node& D=*net.get("DST");
    vector<vector<uint8_t>> used; auto t0=steady_clock::now();
    bool delivered=false; vector<uint8_t> out;

    cl::Optimizer opt;
    telem::ChannelState chan;
    double epoch=1.0;

    for(int step=0; step<260 && !delivered; ++step){
      for(auto& n: net.nodes) n->tick(1.0), n->ingest();

      int have=0; for(auto& p:D.buf) if(p.token_id==token_id) have++;
      double eres=entropy_residual(have,K);

      // Phase-field alignment
      for(auto& t: net.W.ris){
        double ang = atan2(t.p.y-S.pos.y, t.p.x-S.pos.x) + atan2(D.pos.y-t.p.y, D.pos.x-t.p.x);
        double jitter = (eres)*(util::rng.uni()-0.5)*0.9;
        double ph = fmod(ang + jitter, 2*M_PI); int idx = (int)llround(ph/(M_PI/2.0)) & 3; t.phase2b = (uint8_t)idx;
      }
      { vector<uint8_t> ph; ph.reserve(net.W.ris.size()); for(auto&t:net.W.ris) ph.push_back(t.phase2b); HAL::RIS_SET_PHASES(ph); }
      net.W.illum = (eres>0.35? 1.0:0.0);
      if(net.W.illum>0) HAL::CW_ON(0.05); else HAL::CW_OFF();

      double elapsed=duration<double>(steady_clock::now()-t0).count();

      // Stato per optimizer (channel-aware + priority)
      cl::NetworkState Sx{};
      Sx.soc_src=S.bat.soc(); Sx.symbols_have=have; Sx.symbols_need=K;
      Sx.deadline_left_s=max(0.0, I.deadline_s-elapsed);
      Sx.duty_left_rf=HAL::DutyLeftHint();

      double g_probe = net.W.multibounce_best(S.pos, D.pos, 2);
      Sx.chan = chan;
      Sx.chan.push_snr(phy::Mode::RF, phy::snr_db(g_probe, phy::Mode::RF, net.W.illum));
      Sx.chan.push_snr(phy::Mode::BACKSCATTER, phy::snr_db(g_probe, phy::Mode::BACKSCATTER, net.W.illum));
      Sx.chan.push_snr(phy::Mode::IR, phy::snr_db(g_probe, phy::Mode::IR, net.W.illum));
      Sx.decode_rate_symps = (have>0? have / max(1.0, elapsed) : 0.0);

      // priority dal payload (esempio)
      // default NORMAL
      Sx.prio = (Sx.deadline_left_s < I.deadline_s*0.15 ? cl::Priority::CRITICAL
                : Sx.deadline_left_s < I.deadline_s*0.4 ? cl::Priority::NORMAL
                : cl::Priority::BULK);
      Sx.emergency_mode = (Sx.deadline_left_s < I.deadline_s*0.08 && (K-have) > (K*0.25));
      Sx.covert_seq = (uint16_t)((util::now_s() ^ 0x5A) & 0xFF);

      auto dec = opt.joint(I, Sx, epoch);
      double hop = HAL::FHSS_next( (dec.tries>>8) & 0xFF );
      HAL::LORA_CFG(hop, dec.rf_bw_khz, 12, 5, dec.preamble_sym);

      // invio con jitter/spacing + feedback
      int tries_real = (dec.tries & 0xFF);
      uint8_t covert_seq = (dec.tries >> 8) & 0xFF;
      bool emergency = (dec.overhead & 0x8000) != 0;
      int overhead_real = (dec.overhead & 0x7FFF);

      auto do_sleep = [&](int base_ms, int jitter_ms){
        if(base_ms<=0 && jitter_ms<=0) return;
        int extra = (jitter_ms>0 ? (int)(util::rng.uni()*jitter_ms) : 0);
        this_thread::sleep_for(std::chrono::milliseconds(base_ms + extra));
      };

      int ok_cnt=0;
      for(int i=0;i<tries_real; ++i){
        bool ok = S.send_one(net.W, D, dec.mode);
        chan.push_outcome(dec.mode, ok);
        ok_cnt += (ok?1:0);
        do_sleep(dec.min_spacing_ms, dec.jitter_ms);
      }
      if(S.bat.soc()<0.25 && I.allow_backscatter){
        int extra = std::min(8, std::max(2, overhead_real/3));
        for(int k=0;k<extra; ++k){
          bool ok = S.send_one(net.W, D, phy::Mode::BACKSCATTER);
          chan.push_outcome(phy::Mode::BACKSCATTER, ok);
          do_sleep(dec.min_spacing_ms+5, dec.jitter_ms+10);
        }
      }
      double reward = clamp( (double)ok_cnt / max(1, tries_real), 0.0, 1.0 );
      opt.feedback(dec.mode, reward);
      if(emergency){ cout<<"[COVERT] EMERGENCY flag; seq="<<(int)covert_seq<<"\n"; }

      // Fountain decode attempt
      fec::Decoder decod(K, T); int cnt=0;
      for(auto& p: D.buf){ if(p.token_id==token_id){ decod.push(p.fp); used.push_back(p.fp.data); if(++cnt>K*3) break; } }
      auto [ok, raw] = decod.solve();
      if(ok){ out=move(raw); delivered=true; break; }

      if(elapsed > I.deadline_s) break;
      epoch += 1.0;
      this_thread::sleep_for(18ms);
    }

    if(delivered){
      Token rx = bytes2tok(out);
      cout<<"DELIVERED ✓ sig="<<(rx.verify()?"OK":"BAD")<<" payload="<<rx.payload<<"\n";
      vector<string> leaves; leaves.reserve(used.size()); for(auto& s:used) leaves.push_back(podm::leaf(s));
      string root = podm::root(leaves);
      array<uint8_t,32> pk; array<uint8_t,64> sk; CRYPTO::ed25519_keypair(pk.data(), sk.data());
      string msg = bundle_id + token_id + root + to_string(util::now_s());
      array<uint8_t,64> sig; CRYPTO::ed25519_sign(sig.data(), (const uint8_t*)msg.data(), msg.size(), sk.data());
      cout<<"PoD-M root="<<root.substr(0,16)<<"… sig="<<CRYPTO::b64(sig.data(),sig.size()).substr(0,16)<<"…\n";
    } else {
      cout<<"FAILED — aumenta RIS/epsilon o budget duty.\n";
    }

    auto show=[&](const string& id){ Node& n=*net.get(id); size_t have=0; for(auto& p:n.buf) if(p.token_id==token_id) have++; cout<<" - "<<id<<" SoC="<<fixed<<setprecision(0)<<n.bat.soc()*100<<"% buf="<<have<<"\n"; };
    show("SRC"); show("DST");
    cout<<"RIS="<<net.W.ris.size()<<" illum="<<net.W.illum<<"\n";
    return delivered;
  }
};

bool aurora_run(const std::string& intention, Engine* outE = nullptr) {
  ios::sync_with_stdio(false);
  cout<<"=== AURORA-X — Extreme Field Orchestrator ===\n";
  Engine* E = outE ? outE : new Engine();
  E->init(intention);
  bool ok = E->run();
  cout<<(ok? ">>> SUCCESS\n" : ">>> INCOMPLETE — ritenta con più RIS/epsilon\n");
  return ok;
}

#ifndef AURORA_NO_MAIN
int main(){
  std::string intention = "deadline:600; reliability:0.99; duty:0.01; optical:on; backscatter:on; ris:16";
  bool ok = aurora_run(intention);
  return ok?0:1;
}
#endif
