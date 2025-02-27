/*****************************************************************************\
 * Copyright 2014 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#include "resource/evaluators/scoring_api.hpp"

namespace Flux {
namespace resource_model {


/****************************************************************************
 *                                                                          *
 *                    Scoring API Private Method Definitions                *
 *                                                                          *
 ****************************************************************************/

void scoring_api_t::handle_new_keys (const subsystem_t &s,
                                     const std::string &r)
{
    handle_new_subsystem (s);
    handle_new_resrc_type (s, r);
}

void scoring_api_t::handle_new_subsystem (const subsystem_t &s)
{
    if (m_ssys_map.find (s) == m_ssys_map.end ()) {
        auto o = new std::map<const std::string, detail::evals_t *>();
        m_ssys_map.insert (std::make_pair (s, o));
    }
}

void scoring_api_t::handle_new_resrc_type (const subsystem_t &s,
                                           const std::string &r)
{
    if (m_ssys_map[s]->find (r) == m_ssys_map[s]->end ()) {
        auto e = new detail::evals_t (r);
        m_ssys_map[s]->insert (std::make_pair (r, e));
    }
}


/****************************************************************************
 *                                                                          *
 *                    Scoring API Public Method Definitions                 *
 *                                                                          *
 ****************************************************************************/

scoring_api_t::scoring_api_t ()
{

}

scoring_api_t::scoring_api_t (const scoring_api_t &o)
{
    for (auto &p : o.m_ssys_map) {
        const subsystem_t &s = p.first;
        auto obj = new std::map<const std::string, detail::evals_t *>();
        m_ssys_map.insert (std::make_pair (s, obj));
        auto &tmap = *(p.second);
        for (auto &p2 : tmap) {
            const std::string &res_type = p2.first;
            detail::evals_t *ne = new detail::evals_t ();
            *ne = *(p2.second);
            (*m_ssys_map[s]).insert (std::make_pair (res_type, ne));
        }
    }
}

const scoring_api_t &scoring_api_t::operator= (const scoring_api_t &o)
{
    for (auto &p : o.m_ssys_map) {
        const subsystem_t &s = p.first;
        auto obj = new std::map<const std::string, detail::evals_t *>();
        m_ssys_map.insert (std::make_pair (s, obj));
        auto &tmap = *(p.second);
        for (auto &p2 : tmap) {
            const std::string &res_type = p2.first;
            detail::evals_t *ne = new detail::evals_t ();
            *ne = *(p2.second);
            (*m_ssys_map[s]).insert (std::make_pair (res_type, ne));
        }
    }
    return *this;
}

scoring_api_t::~scoring_api_t ()
{
    auto i = m_ssys_map.begin ();
    while (i != m_ssys_map.end ()) {
        auto tmap = i->second;
        auto j = tmap->begin ();
        while (j != tmap->end ()) {
            delete j->second;
            j = tmap->erase (j);
        }
        delete i->second;
        i = m_ssys_map.erase (i);
    }
}

int64_t scoring_api_t::cutline (const subsystem_t &s, const std::string &r)
{
    handle_new_keys (s, r);
    auto res_evals = (*m_ssys_map[s])[r];
    return res_evals->cutline ();
}

int64_t scoring_api_t::set_cutline (const subsystem_t &s, const std::string &r,
                                    int64_t c)
{
    handle_new_keys (s, r);
    auto res_evals = (*m_ssys_map[s])[r];
    return res_evals->set_cutline (c);
}


void scoring_api_t::eval_egroups_iter_reset (const subsystem_t &s,
                                             const std::string &r)
{
    handle_new_keys (s, r);
    auto res_evals = (*m_ssys_map[s])[r];
    res_evals->eval_egroups_iter_reset ();
}

std::vector<eval_egroup_t>::iterator scoring_api_t::eval_egroups_iter_next (
                                                        const subsystem_t &s,
                                                        const std::string &r)
{
    handle_new_keys (s, r);
    auto res_evals = (*m_ssys_map[s])[r];
    return res_evals->eval_egroups_iter_next ();
}

std::vector<eval_egroup_t>::iterator scoring_api_t::eval_egroups_end (
                                                        const subsystem_t &s,
                                                        const std::string &r)
{
    handle_new_keys (s, r);
    auto res_evals = (*m_ssys_map[s])[r];
    return res_evals->eval_egroups_end ();
}

int scoring_api_t::add (const subsystem_t &s, const std::string &r,
                        const eval_egroup_t &eg)
{
    handle_new_keys (s, r);
    auto res_evals = (*m_ssys_map[s])[r];
    return res_evals->add (eg);
}

//! Can throw an out_of_range exception
const eval_egroup_t &scoring_api_t::at (const subsystem_t &s,
                                        const std::string &r, unsigned int i)
{
    handle_new_keys (s, r);
    auto res_evals = (*m_ssys_map[s])[r];
    return res_evals->at(i);
}

unsigned int scoring_api_t::qualified_count (const subsystem_t &s,
                                             const std::string &r)
{
    handle_new_keys (s, r);
    auto res_evals = (*m_ssys_map[s])[r];
    return res_evals->qualified_count ();
}

unsigned int scoring_api_t::qualified_granules (const subsystem_t &s,
                                                const std::string &r)
{
    handle_new_keys (s, r);
    auto res_evals = (*m_ssys_map[s])[r];
    return res_evals->qualified_granules ();
}

unsigned int scoring_api_t::total_count (const subsystem_t &s,
                                         const std::string &r)
{
    handle_new_keys (s, r);
    auto res_evals = (*m_ssys_map[s])[r];
    return res_evals->total_count ();
}

unsigned int scoring_api_t::best_k (const subsystem_t &s, const std::string &r)
{
    handle_new_keys (s, r);
    auto res_evals = (*m_ssys_map[s])[r];
    return res_evals->best_k ();
}

unsigned int scoring_api_t::best_i (const subsystem_t &s, const std::string &r)
{
    handle_new_keys (s, r);
    auto res_evals = (*m_ssys_map[s])[r];
    return res_evals->best_i ();
}

bool scoring_api_t::hier_constrain_now ()
{
    return m_hier_constrain_now;
}

void scoring_api_t::merge (const scoring_api_t &o)
{
    for (auto &kv : o.m_ssys_map) {
        const subsystem_t &s = kv.first;
        auto &tmap = *(kv.second);
        for (auto &kv2 : tmap) {
            const std::string &r = kv2.first;
            auto &ev = *(kv2.second);
            handle_new_keys (s, r);
            auto res_evals = (*m_ssys_map[s])[r];
            res_evals->merge (ev);
        }
    }
}

void scoring_api_t::resrc_types (const subsystem_t &s,
                                 std::vector<std::string> &v)
{
    handle_new_subsystem (s);
    for (auto &kv : *(m_ssys_map[s]))
        v.push_back (kv.first);
}

// overall_score and avail are temporary space such that
// a child vertex visitor can pass the info to the parent vertex
int64_t scoring_api_t::overall_score ()
{
    return m_overall_score;
}

void scoring_api_t::set_overall_score (int64_t overall)
{
    m_overall_score = overall;
}

unsigned int scoring_api_t::avail ()
{
    return m_avail;
}

void scoring_api_t::set_avail (unsigned int avail)
{
    m_avail = avail;
}

bool scoring_api_t::is_contained (const subsystem_t &s,
                                  const std::string &r)
{
    bool rc = false;
    if (m_ssys_map.find (s) != m_ssys_map.end ()) {
        if (m_ssys_map[s]->find (r) != m_ssys_map[s]->end ()) {
            rc = true;
        }
    }
    return rc;
}


} // Flux::resource_model
} // Flux

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
