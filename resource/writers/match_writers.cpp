/*****************************************************************************\
 * Copyright 2014 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#include <new>
#include <cerrno>
#include <algorithm>
#include <iterator>
extern "C" {
#include <flux/hostlist.h>
}
#include "resource/schema/resource_data.hpp"
#include "resource/schema/resource_graph.hpp"
#include "resource/writers/match_writers.hpp"

namespace Flux {
namespace resource_model {


/****************************************************************************
 *                                                                          *
 *              Base Writers Class Public Method Definitions                *
 *                                                                          *
 ****************************************************************************/

int match_writers_t::compress_ids (std::stringstream &o,
                                   const std::vector<int64_t> &ids)
{
    int rc = 0;
    try {
        int64_t base = INT64_MIN;
        int64_t runlen = 0;

        for (auto &id : ids) {
            if (id == (base + runlen + 1)) {
                runlen++;
            } else {
                if (runlen != 0)
                    o << "-" + std::to_string (base + runlen) + ",";
                else if (base != INT64_MIN)
                    o << ",";
                o << id;
                base = id;
                runlen = 0;
            }
        }
        if (runlen)
            o << "-" << (base + runlen);
    } catch (std::bad_alloc &) {
        rc = -1;
        errno = ENOMEM;
    }
    return rc;
}

int match_writers_t::compress_hosts (const std::vector<std::string> &hosts,
                                     const char *hostlist_init,
                                     char **hostlist_out)
{
    int rc = 0;
    hostlist *hl = NULL;

    if (!hostlist_out) {
        rc = -1;
        errno = EINVAL;
        goto ret;
    }
    if (hostlist_init) {
        if ( !(hl = hostlist_decode (hostlist_init))) {
            rc = -1;
            goto ret;
        }
    } else {
        if ( !(hl = hostlist_create ())) {
            rc = -1;
            goto ret;
        }
    }
    for (auto &hn : hosts) {
        if ( (rc = hostlist_append (hl, hn.c_str ())) < 0)
            goto ret;
    }

    if ( !(*hostlist_out = hostlist_encode (hl))) {
        rc = -1;
        goto ret;
    }

ret:
    hostlist_destroy (hl);
    return rc;
}


/****************************************************************************
 *                                                                          *
 *              Simple Writers Class Public Method Definitions              *
 *                                                                          *
 ****************************************************************************/

bool sim_match_writers_t::empty ()
{
    return m_out.str ().empty ();
}

int sim_match_writers_t::emit_json (json_t **j_o, json_t **aux)
{
    int rc = 0;
    json_t *o = NULL;
    std::string str = m_out.str ();

    if (!str.empty ()) {
        if ( !(o = json_string (str.c_str ()))) {
            errno = ENOMEM;
            rc = -1;
            goto done;
        }
    }
    *j_o = o;

done:
    m_out.str ("");
    m_out.clear ();
    return rc;
}

int sim_match_writers_t::emit (std::stringstream &out)
{
    out << m_out.str ();
    m_out.str ("");
    m_out.clear ();
    return 0;
}

int sim_match_writers_t::emit_vtx (const std::string &prefix,
                                   const f_resource_graph_t &g, const vtx_t &u,
                                   unsigned int needs, bool exclusive)
{
    std::string mode = (exclusive)? "x" : "s";
    m_out << prefix << g[u].name << "[" << needs << ":" << mode  << "]"
          << std::endl;
    return 0;
}


/****************************************************************************
 *                                                                          *
 *       JSON Graph Format (JGF) Writers Classs Public Definitions          *
 *                                                                          *
 ****************************************************************************/

jgf_match_writers_t::jgf_match_writers_t ()
{
    if (!(m_vout = json_array ()))
        throw std::bad_alloc ();
    if (!(m_eout = json_array ())) {
        json_decref (m_vout);
        m_vout = NULL;
        throw std::bad_alloc ();
    }
}

jgf_match_writers_t::jgf_match_writers_t (const jgf_match_writers_t &w)
{
    if (!(m_vout = json_deep_copy (w.m_vout)))
        throw std::bad_alloc ();
    if (!(m_eout = json_deep_copy (w.m_eout))) {
        json_decref (m_vout);
        m_vout = NULL;
        throw std::bad_alloc ();
    }
}

jgf_match_writers_t &jgf_match_writers_t::operator=(
                                              const jgf_match_writers_t &w)
{
    if (!(m_vout = json_deep_copy (w.m_vout)))
        throw std::bad_alloc ();
    if (!(m_eout = json_deep_copy (w.m_eout))) {
        json_decref (m_vout);
        m_vout = NULL;
        throw std::bad_alloc ();
    }
    return *this;
}

jgf_match_writers_t::~jgf_match_writers_t ()
{
    json_decref (m_vout);
    json_decref (m_eout);
}

bool jgf_match_writers_t::empty ()
{
    size_t v_size = json_array_size (m_vout);
    size_t e_size = json_array_size (m_eout);
    return (v_size == 0) && (e_size == 0);
}

int jgf_match_writers_t::emit_json (json_t **o, json_t **aux)
{
    int rc = 0;

    if ((rc = check_array_sizes ()) <= 0)
        goto ret;
    if (!(*o = json_pack ("{s:{s:o s:o}}",
                              "graph",
                                  "nodes", m_vout,
                                  "edges", m_eout))) {
        json_decref (m_vout);
        json_decref (m_eout);
        m_vout = NULL;
        m_eout = NULL;
        rc = -1;
        errno = ENOMEM;
        goto ret;
    }
    m_vout = NULL;
    m_eout = NULL;
    if ((alloc_json_arrays ()) < 0) {
        json_decref (*o);
        rc = -1;
        *o = NULL;
        goto ret;
    }

ret:
    return rc;
}

int jgf_match_writers_t::emit (std::stringstream &out)
{
    int rc = 0;
    json_t *o = NULL;
    if ((rc = emit_json (&o)) > 0) {
        char *json_str = NULL;
        if (!(json_str = json_dumps (o, JSON_INDENT (0)))) {
            json_decref (o);
            o = NULL;
            rc = -1;
            errno = ENOMEM;
            goto ret;
        }
        out << json_str << std::endl;
        free (json_str);
        json_decref (o);
    }
ret:
    return (rc == -1)? -1 : 0;
}

int jgf_match_writers_t::emit_vtx (const std::string &prefix,
                                   const f_resource_graph_t &g, const vtx_t &u,
                                   unsigned int needs, bool exclusive)
{
    int rc = 0;
    json_t *o = NULL;
    json_t *b = NULL;
    auto &ephemeral = g[u].idata.ephemeral;
    auto &eph_map = ephemeral.to_map ();

    if (!m_vout || !m_eout) {
        rc = -1;
        errno = EINVAL;
        goto out;
    }
    if (!(b = emit_vtx_base (g, u, needs, exclusive))) {
        rc = -1;
        goto out;
    }
    if ((rc = map2json (b, g[u].properties, "properties") < 0)) {
        json_decref (b);
        goto out;
    }
    if ((rc = map2json (b, g[u].paths, "paths") < 0)) {
        json_decref (b);
        goto out;
    }
    if ((rc = map2json (b, eph_map, "ephemeral") < 0)) {
        json_decref (b);
        goto out;
    }
    if ((o = json_pack ("{s:s s:o}",
                            "id", std::to_string (g[u].uniq_id).c_str (),
                            "metadata", b)) == NULL) {
        json_decref (b);
        rc = -1;
        errno = ENOMEM;
        goto out;
    }
    if ((rc = json_array_append_new (m_vout, o)) < 0) {
        errno = ENOMEM;
        goto out;
    }

out:
    return rc;
}

int jgf_match_writers_t::emit_edg (const std::string &prefix,
                                   const f_resource_graph_t &g, const edg_t &e)
{
    int rc = 0;
    json_t *o = NULL;
    json_t *m = NULL;

    if (!m_eout || !m_eout) {
        rc = -1;
        errno = EINVAL;
        goto out;
    }
    if (!(m = json_object ())) {
        rc = -1;
        errno = ENOMEM;
        goto out;
    }
    if ((rc = emit_edg_meta (m, g, e)) < 0) {
        json_decref (m);
        goto out;
    }
    if (!(o = json_pack ("{s:s s:s s:o}",
                  "source", std::to_string (g[source (e, g)].uniq_id).c_str (),
                  "target", std::to_string (g[target (e, g)].uniq_id).c_str (),
                  "metadata", m))) {
        json_decref (m);
        rc = -1;
        errno = ENOMEM;
        goto out;
    }
    if ((rc = json_array_append_new (m_eout, o)) == -1) {
        errno = ENOMEM;
        goto out;
    }

out:
    return rc;
}


/****************************************************************************
 *                                                                          *
 *       JSON Graph Format (JGF) Writers Classs Private Definitions         *
 *                                                                          *
 ****************************************************************************/

int jgf_match_writers_t::alloc_json_arrays ()
{
    int rc = -1;

    if (m_vout || m_eout)
        goto ret;
    if (!(m_vout = json_array ())) {
        errno = ENOMEM;
        goto ret;
    }
    if (!(m_eout = json_array ())) {
        json_decref (m_vout);
        errno = ENOMEM;
        goto ret;
    }
    rc = 0;
ret:
    return rc;
}

int jgf_match_writers_t::check_array_sizes ()
{
    int rc = 0;
    int s1 = 0;
    int s2 = 0;

    if (!m_vout || !m_eout) {
        rc = -1;
        errno = EINVAL;
        goto ret;
    }

    s1 = json_array_size (m_vout);
    s2 = json_array_size (m_eout);
    rc = s1 + s2;
    if (s1 == 0 && s2 == 0)
        goto ret;
    if (s1 == 0 || s2 == 0) {
        rc = -1;
        errno = ENOENT;
        goto ret;
    }
ret:
    return rc;
}

json_t *jgf_match_writers_t::emit_vtx_base (const f_resource_graph_t &g,
                                            const vtx_t &u,
                                            unsigned int needs, bool exclusive)
{
    json_t *o = NULL;

    if (!(o = json_pack ("{s:s s:s s:s s:I s:I s:i s:b s:s s:I}",
                            "type", g[u].type.c_str (),
                            "basename", g[u].basename.c_str (),
                            "name", g[u].name.c_str (),
                            "id", g[u].id,
                            "uniq_id", g[u].uniq_id,
                            "rank", g[u].rank,
                            "exclusive", (exclusive)? 1 : 0,
                            "unit", g[u].unit.c_str (),
                            "size", static_cast<int64_t> (needs)))) {
        errno = ENOMEM;
    }
    return o;
}

int jgf_match_writers_t::map2json (json_t *o,
                                   const std::map<std::string, std::string> &mp,
                                   const char *key)
{
    int rc = 0;
    if (!mp.empty ()) {
        json_t *p = NULL;
        if (!(p = json_object ())) {
            rc = -1;
            errno = ENOMEM;
            goto out;
        }
        for (auto &kv : mp) {
            json_t *vo = NULL;
            if (!(vo = json_string (kv.second.c_str ()))) {
                json_decref (p);
                rc = -1;
                errno = ENOMEM;
                goto out;
            }
            if ((rc = json_object_set_new (p, kv.first.c_str (), vo)) == -1) {
                json_decref (p);
                errno = ENOMEM;
                goto out;
            }
         }
        if ((rc = json_object_set_new (o, key, p)) == -1) {
            errno = ENOMEM;
            goto out;
        }
    }

out:
    return rc;
}

int jgf_match_writers_t::emit_edg_meta (json_t *o, const f_resource_graph_t &g,
                                        const edg_t &e)
{
    int rc = 0;
    if (!o) {
        rc = -1;
        errno = EINVAL;
        goto out;
    }
    if (!g[e].name.empty ()) {
        json_t *n = NULL;
        if (!(n = json_object ())) {
            rc = -1;
            errno = ENOMEM;
            goto out;
        }
        for (auto &kv : g[e].name) {
            json_t *vo = NULL;
            if (!(vo = json_string (kv.second.c_str ()))) {
                json_decref (n);
                rc = -1;
                errno = ENOMEM;
                goto out;
            }
            if ((rc = json_object_set_new (n, kv.first.c_str (), vo))) {
                json_decref (n);
                errno = ENOMEM;
                goto out;
            }
        }
        if ((rc = json_object_set_new (o, "name", n)) == -1) {
            errno = ENOMEM;
            goto out;
        }
    }

out:
    return rc;
}


/****************************************************************************
 *                                                                          *
 *            RLITE Writers Class Public Method Definitions                 *
 *                                                                          *
 ****************************************************************************/

rlite_match_writers_t::rlite_match_writers_t ()
{
    m_reducer["core"] = std::vector<int64_t> ();
    m_reducer["gpu"] = std::vector<int64_t> ();
    m_gatherer.insert ("node");
}

rlite_match_writers_t::rlite_match_writers_t (const rlite_match_writers_t &w)
{
    m_reducer = w.m_reducer;
    m_gatherer = w.m_gatherer;
    m_gl_gatherer = w.m_gl_gatherer;
}

rlite_match_writers_t &rlite_match_writers_t::operator=(
                                                 const rlite_match_writers_t &w)
{
    m_reducer = w.m_reducer;
    m_gatherer = w.m_gatherer;
    m_gl_gatherer = w.m_gl_gatherer;
    return *this;
}

rlite_match_writers_t::~rlite_match_writers_t ()
{
}

bool rlite_match_writers_t::empty ()
{
    return m_gl_gatherer.empty ();
}

int rlite_match_writers_t::emit_json (json_t **o, json_t **aux)
{
    int rc = 0;
    int saved_errno;
    json_t *rlite_array = NULL;
    json_t *host_array = NULL;

    if (m_gl_gatherer.empty ()) {
        errno = EINVAL;
        rc = -1;
        goto ret;
    }
    if ( !(rlite_array = json_array ())) {
        rc = -1;
        errno = ENOMEM;
        goto ret;
    }
    if ( aux && !(host_array = json_array ())) {
        json_decref (rlite_array);
        rc = -1;
        errno = ENOMEM;
        goto ret;
    }
    if ( (rc = fill (rlite_array, host_array)) < 0) {
        saved_errno = errno;
        json_decref (rlite_array);
        if (host_array)
            json_decref (host_array);
        errno = saved_errno;
        goto ret;

    }

    m_gl_gatherer.clear ();

    if ( (rc = json_array_size (rlite_array)) != 0) {
        *o = rlite_array;
        if (aux)
            *aux = host_array;
    }

ret:
    return rc;
}

int rlite_match_writers_t::emit (std::stringstream &out, bool newline)
{
    int rc = 0;
    json_t *o = NULL;
    if ((rc = emit_json (&o)) > 0) {
        char *json_str = NULL;
        if (!(json_str = json_dumps (o, JSON_INDENT (0)))) {
            json_decref (o);
            rc = -1;
            errno = ENOMEM;
            goto ret;
        }
        out << json_str;
        if (newline)
            out << std::endl;
        free (json_str);
        json_decref (o);
     }
ret:
    return (rc == -1)? -1 : 0;
}

int rlite_match_writers_t::emit (std::stringstream &out)
{
    return emit (out, true);
}

int rlite_match_writers_t::emit_vtx (const std::string &prefix,
                                     const f_resource_graph_t &g,
                                     const vtx_t &u,
                                     unsigned int needs,
                                     bool exclusive)
{
    int rc = 0;

    if (m_reducer.find (g[u].type) != m_reducer.end ()) {
        try {
            m_reducer[g[u].type].push_back (g[u].id);
        } catch (std::bad_alloc &) {
            rc = -1;
            errno = ENOMEM;
            goto ret;
        }
    } else if (m_gatherer.find (g[u].type) != m_gatherer.end ()) {
        if ((rc = emit_gatherer (g, u)) < 0)
            goto ret;
    }
ret:
    return rc;
}


/****************************************************************************
 *                                                                          *
 *            RLITE Writers Class Private Method Definitions                *
 *                                                                          *
 ****************************************************************************/

bool rlite_match_writers_t::m_reducer_set ()
{
    bool set = false;
    for (auto &kv : m_reducer) {
        if (!kv.second.empty ()) {
            set = true;
            break;
        }
    }
    return set;
}

int rlite_match_writers_t::get_gatherer_children (std::string &children)
{
    int rc = 0;
    int saved_errno;
    json_t *co = NULL;
    char *co_dump = NULL;

    if (!(co = json_object ())) {
        rc = -1;
        errno = ENOMEM;
        goto ret;
    }
    for (auto &kv : m_reducer) {
        json_t *vo = NULL;
        std::stringstream s;
        if (kv.second.empty ())
            continue;

        std::sort (kv.second.begin (), kv.second.end ());
        if ( (rc = compress_ids (s, kv.second)) < 0)
            goto memfree_ret;
        if ( !(vo = json_string (s.str ().c_str ()))) {
            rc = -1;
            errno = ENOMEM;
            goto memfree_ret;
        }
        if ( (rc = json_object_set_new (co, kv.first.c_str (), vo)) < 0) {
            errno = ENOMEM;
            goto memfree_ret;
        }
        kv.second.clear ();
    }
    if (!(co_dump = json_dumps (co, JSON_INDENT (0)))) {
        rc = -1;
        errno = ENOMEM;
        goto memfree_ret;
    }

    children = co_dump;

memfree_ret:
    saved_errno = errno;
    free (co_dump);
    json_decref (co);
    errno = saved_errno;
ret:
    return rc;
}

int rlite_match_writers_t::emit_gatherer (const f_resource_graph_t &g,
                                          const vtx_t &u)
{
    int rc = 0;
    std::string children;

    try {
        if (!m_reducer_set ())
            goto ret;

        if ( (rc = get_gatherer_children (children)) < 0)
            goto ret;

        if (m_gl_gatherer.find (children) == m_gl_gatherer.end ())
            m_gl_gatherer[children] = std::vector<rank_host_t> ();

        m_gl_gatherer[children].push_back ({g[u].rank, g[u].name});
    } catch (std::bad_alloc &) {
        rc = -1;
        errno = ENOMEM;
    }

ret:
    return rc;
}

int rlite_match_writers_t::fill (json_t *rlite_array, json_t *host_array)
{
    int rc = 0;
    int saved_errno;
    char *hl_out = nullptr;
    std::vector<rank_host_t> rankhosts_vec;


    // m_gl_gatherer is keyed by the "children" signature of each node
    // the value is the set of ranks that have the same signature.
    for (auto &kv : m_gl_gatherer) {
        json_error_t err;
        std::stringstream s;
        std::vector<int64_t> ranks_vec;
        json_t *robj, *ranks, *cobj = NULL;

        // The primary sorting order for compression is rank
        std::sort (kv.second.begin (), kv.second.end (),
                   [] (const rank_host_t &a, const rank_host_t &b) {
                       return a.rank < b.rank;
                   });
        std::transform (kv.second.begin (), kv.second.end (),
                        std::back_inserter (ranks_vec),
                        [] (rank_host_t &o) {
                            return o.rank;
                        });
        if ( (rc = compress_ids (s, ranks_vec)) < 0)
            goto ret;
        if (host_array)
            std::copy (kv.second.begin (),
                       kv.second.end (), std::back_inserter (rankhosts_vec));
        if ( !(robj = json_object ())) {
            rc = -1;
            errno = ENOMEM;
            goto ret;
        }
        if ( !(ranks = json_string (s.str ().c_str ()))) {
            json_decref (robj);
            rc = -1;
            errno = ENOMEM;
            goto ret;
        }
        if ( (rc = json_object_set_new (robj, "rank", ranks)) < 0) {
            json_decref (robj);
            errno = ENOMEM;
            goto ret;
        }
        if ( !(cobj = json_loads (kv.first.c_str (), 0, &err))) {
            json_decref (robj);
            rc = -1;
            errno = ENOMEM;
            goto ret;
        }
        if ( (rc = json_object_set_new (robj, "children", cobj)) < 0) {
            json_decref (robj);
            errno = ENOMEM;
            goto ret;
        }
        if ( (rc = json_array_append_new (rlite_array, robj)) < 0) {
            json_decref (robj);
            errno = ENOMEM;
            goto ret;
        }
    }

    if (host_array) {
        // The primary sorting order for compression is rank
        std::sort (rankhosts_vec.begin (), rankhosts_vec.end (),
                   [] (const rank_host_t &a, const rank_host_t &b) {
                       return a.rank < b.rank;
                   });
        std::vector<std::string> hosts_vec;
        std::transform (rankhosts_vec.begin (), rankhosts_vec.end (),
                        std::back_inserter (hosts_vec),
                        [] (rank_host_t &o) {
                            return o.host;
                        });
        if ( (rc = compress_hosts (hosts_vec, nullptr, &hl_out)) < 0)
            goto ret;
        if (hl_out) {
            if ( (rc = json_array_append_new (host_array,
                                              json_string (hl_out))) < 0) {
                json_decref (rlite_array);
                errno = ENOMEM;
                goto ret;
            }
        }
    }

ret:
    saved_errno = errno;
    free (hl_out);
    errno = saved_errno;
    return rc;
}


/****************************************************************************
 *                                                                          *
 *                  RV1 Writers Class Method Definitions                    *
 *                                                                          *
 ****************************************************************************/

bool rv1_match_writers_t::empty ()
{
    return (rlite.empty () && jgf.empty ());
}

int rv1_match_writers_t::attrs_json (json_t **o) {
    int rc = 0;
    json_t *sys = NULL;
    json_t *attrs = json_object ();
    for (const auto &kv : m_attrs) {
        json_t *stro = NULL;
        if (!(stro = json_string (kv.second.c_str ()))) {
            json_decref (attrs);
            rc = -1;
            errno = ENOMEM;
            goto ret;
        }
        if ((rc = json_object_set_new (attrs, kv.first.c_str (), stro)) == -1) {
            json_decref (attrs);
            rc = -1;
            errno = ENOMEM;
            goto ret;
        }
    }
    if (!(sys = json_pack ("{s:{s:o}}",
                               "system",
                                   "scheduler", attrs))) {
        json_decref (attrs);
        rc = -1;
        errno = ENOMEM;
        goto ret;
    }
    *o = sys;

ret:
    return rc;
}

int rv1_match_writers_t::emit_json (json_t **j_o, json_t **aux)
{
    int rc = 0;
    int saved_errno;
    json_t *o = NULL;
    json_t *rlite_o = NULL;
    json_t *ndlist_o = NULL;
    json_t *jgf_o = NULL;
    json_t *attrs_o = NULL;

    if (rlite.empty () || jgf.empty ())
        goto ret;
    if ( (rc = rlite.emit_json (&rlite_o, &ndlist_o)) < 0)
        goto ret;
    if ( (rc = jgf.emit_json (&jgf_o)) < 0) {
        saved_errno = errno;
        json_decref (rlite_o);
        json_decref (ndlist_o);
        errno = saved_errno;
        goto ret;
    }
    if ( !(o = json_pack ("{s:i s:{s:o s:o s:I s:I} s:o}",
                              "version", 1,
                              "execution",
                                  "R_lite", rlite_o,
                                  "nodelist", ndlist_o,
                                  "starttime", m_starttime,
                                  "expiration", m_expiration,
                              "scheduling", jgf_o))) {
        json_decref (rlite_o);
        json_decref (ndlist_o);
        json_decref (jgf_o);
        rc = -1;
        errno = ENOMEM;
        goto ret;
    }
    if (!m_attrs.empty ()) {
        if ((rc = attrs_json (&attrs_o)) < 0) {
            saved_errno = errno;
            json_decref (o);
            errno = saved_errno;
            goto ret;
        }
        if ( (rc = json_object_set_new (o, "attributes", attrs_o)) == -1) {
            json_decref (o);
            errno = ENOMEM;
            goto ret;
        }
    }
    *j_o = o;

ret:
    return rc;
}

int rv1_match_writers_t::emit (std::stringstream &out)
{
    int rc = 0;
    json_t *o = NULL;
    char *json_str = NULL;

    if (rlite.empty () || jgf.empty ())
        goto ret;
    if ( (rc = emit_json (&o)) < 0)
        goto ret;
    if ( !(json_str = json_dumps (o, JSON_INDENT (0)))) {
        json_decref (o);
        o = NULL;
        rc = -1;
        errno = ENOMEM;
        goto ret;
    }
    out << json_str << std::endl;
    free (json_str);
    json_decref (o);

ret:
    return rc;
}

int rv1_match_writers_t::emit_vtx (const std::string &prefix,
                                   const f_resource_graph_t &g,
                                   const vtx_t &u,
                                   unsigned int needs,
                                   bool exclusive)
{
    int rc = 0;
    if ((rc = rlite.emit_vtx (prefix, g, u, needs, exclusive)) == 0)
        rc = jgf.emit_vtx (prefix, g, u, needs, exclusive);
    return rc;
}

int rv1_match_writers_t::emit_edg (const std::string &prefix,
                                   const f_resource_graph_t &g,
                                   const edg_t &e)
{
    int rc = 0;
    if ((rc = rlite.emit_edg (prefix, g, e)) == 0)
        rc = jgf.emit_edg (prefix, g, e);
    return rc;
}

int rv1_match_writers_t::emit_tm (uint64_t start_tm, uint64_t end_tm)
{
    m_starttime = start_tm;
    m_expiration = end_tm;
    return 0;
}

int rv1_match_writers_t::emit_attrs (const std::string &k, const std::string &v)
{
    m_attrs[k] = v;
    return 0;
}


/****************************************************************************
 *                                                                          *
 *              RV1 Nosched Writers Class Method Definitions                *
 *                                                                          *
 ****************************************************************************/

bool rv1_nosched_match_writers_t::empty ()
{
    return rlite.empty ();
}

int rv1_nosched_match_writers_t::emit_json (json_t **j_o, json_t **aux)
{
    int rc = 0;
    json_t *rlite_o = NULL;
    json_t *ndlist_o = NULL;

    if (rlite.empty ())
        goto ret;
    if ( (rc = rlite.emit_json (&rlite_o, &ndlist_o)) < 0)
        goto ret;
    if ( !(*j_o = json_pack ("{s:i s:{s:o s:o s:I s:I}}",
                                "version", 1,
                                "execution",
                                    "R_lite",  rlite_o,
                                    "nodelist", ndlist_o,
                                    "starttime", m_starttime,
                                    "expiration", m_expiration))) {
        json_decref (rlite_o);
        json_decref (ndlist_o);
        rc = -1;
        errno = ENOMEM;
        goto ret;
    }

ret:
    return rc;
}

int rv1_nosched_match_writers_t::emit (std::stringstream &out)
{
    int rc = 0;
    json_t *o = NULL;
    char *json_str = NULL;

    if (rlite.empty ())
        goto ret;
    if ( (rc = emit_json (&o)) < 0)
        goto ret;
    if ( !(json_str = json_dumps (o, JSON_INDENT (0)))) {
        json_decref (o);
        o = NULL;
        rc = -1;
        errno = ENOMEM;
        goto ret;
    }
    out << json_str << std::endl;
    free (json_str);
    json_decref (o);

ret:
    return rc;
}

int rv1_nosched_match_writers_t::emit_vtx (const std::string &prefix,
                                           const f_resource_graph_t &g,
                                           const vtx_t &u,
                                           unsigned int needs,
                                           bool exclusive)
{
    return rlite.emit_vtx (prefix, g, u, needs, exclusive);
}

int rv1_nosched_match_writers_t::emit_tm (uint64_t start_tm, uint64_t end_tm)
{
    m_starttime = start_tm;
    m_expiration = end_tm;
    return 0;
}


/****************************************************************************
 *                                                                          *
 *         PRETTY Simple Writers Class Public Method Definitions            *
 *                                                                          *
 ****************************************************************************/

bool pretty_sim_match_writers_t::empty ()
{
    bool empty = true;
    for (auto &s: m_out) {
        if (!s.empty ()) {
            empty = false;
            break;
        }
    }
    return empty;
}

int pretty_sim_match_writers_t::emit_json (json_t **j_o, json_t **aux)
{
    json_t *o = NULL;
    std::string str = "";

    for (auto &s: m_out)
        str += s;
    m_out.clear ();
    if (!str.empty ()) {
        if ( !(o = json_string (str.c_str ()))) {
            errno = ENOMEM;
            return -1;
        }
    }
    *j_o = o;
    return 0;
}

int pretty_sim_match_writers_t::emit (std::stringstream &out)
{
    for (auto &s: m_out)
        out << s;
    m_out.clear ();
    return 0;
}

int pretty_sim_match_writers_t::emit_vtx (const std::string &prefix,
                                          const f_resource_graph_t &g,
                                          const vtx_t &u,
                                          unsigned int needs, bool exclusive)
{
    std::stringstream out;
    std::string mode = (exclusive)? "exclusive" : "shared";
    out << prefix << g[u].name << "[" << needs << ":" << mode  << "]"
        << std::endl;
    m_out.push_front (out.str ());
    return 0;
}


/****************************************************************************
 *                                                                          *
 *             Match Writers Factory Class Method Definitions               *
 *                                                                          *
 ****************************************************************************/

std::shared_ptr<match_writers_t> match_writers_factory_t::
                                     create (match_format_t f)
{
    std::shared_ptr<match_writers_t> w = nullptr;

    try {
        switch (f) {
        case match_format_t::SIMPLE:
            w = std::make_shared<sim_match_writers_t> ();
            break;
        case match_format_t::JGF:
            w = std::make_shared<jgf_match_writers_t> ();
            break;
        case match_format_t::RLITE:
            w = std::make_shared<rlite_match_writers_t> ();
            break;
        case match_format_t::RV1_NOSCHED:
            w = std::make_shared<rv1_nosched_match_writers_t> ();
            break;
        case match_format_t::PRETTY_SIMPLE:
            w = std::make_shared<pretty_sim_match_writers_t> ();
            break;
        case match_format_t::RV1:
        default:
            w = std::make_shared<rv1_match_writers_t> ();
            break;
        }
    } catch (std::bad_alloc &e) {
        errno = ENOMEM;
        w = nullptr;
    }

    return w;
}

match_format_t match_writers_factory_t::get_writers_type (const std::string &n)
{
    match_format_t format = match_format_t::SIMPLE;
    if (n == "jgf")
        format = match_format_t::JGF;
    else if (n == "rlite")
        format = match_format_t::RLITE;
    else if (n == "rv1")
        format = match_format_t::RV1;
    else if (n == "rv1_nosched")
        format = match_format_t::RV1_NOSCHED;
    else if (n == "pretty_simple")
        format = match_format_t::PRETTY_SIMPLE;
    return format;
}

bool known_match_format (const std::string &format)
{
   return (format == "simple"
           || format == "jgf"
           || format == "rlite"
           || format == "rv1"
           || format == "rv1_nosched"
           || format == "pretty_simple");
}

} // namespace resource_model
} // namespace Flux

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
