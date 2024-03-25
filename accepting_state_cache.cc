#pragma once
#include "common.hh"

class AcceptingStateCache {
private:
    twa_graph_ptr g;
    std::vector<bool> cache;

    bool check_accepting_state_nocache(const unsigned state) const {
        for (auto& t: g->out(state)) {
            for (auto v: t.acc.sets())
                return true;
            break;
        }
        return false;
    }

    void init_accepting_state_cache() {
        cache.resize(g->num_states(), false);
        for (unsigned i=0; i<g->num_states(); ++i) {
            cache[i] = check_accepting_state_nocache(i);
        }
    }
public:
    AcceptingStateCache(twa_graph_ptr g) : g{g} {
        init_accepting_state_cache();
    }

    bool accepting(const unsigned state) const {
        return cache[state];
    }
};