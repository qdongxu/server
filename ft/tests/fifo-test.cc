/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
/*
COPYING CONDITIONS NOTICE:

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation, and provided that the
  following conditions are met:

      * Redistributions of source code must retain this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below).

      * Redistributions in binary form must reproduce this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below) in the documentation and/or other materials
        provided with the distribution.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
  02110-1301, USA.

COPYRIGHT NOTICE:

  TokuDB, Tokutek Fractal Tree Indexing Library.
  Copyright (C) 2007-2013 Tokutek, Inc.

DISCLAIMER:

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

UNIVERSITY PATENT NOTICE:

  The technology is licensed by the Massachusetts Institute of
  Technology, Rutgers State University of New Jersey, and the Research
  Foundation of State University of New York at Stony Brook under
  United States of America Serial No. 11/760379 and to the patents
  and/or patent applications resulting from it.

PATENT MARKING NOTICE:

  This software is covered by US Patent No. 8,185,551.
  This software is covered by US Patent No. 8,489,638.

PATENT RIGHTS GRANT:

  "THIS IMPLEMENTATION" means the copyrightable works distributed by
  Tokutek as part of the Fractal Tree project.

  "PATENT CLAIMS" means the claims of patents that are owned or
  licensable by Tokutek, both currently or in the future; and that in
  the absence of this license would be infringed by THIS
  IMPLEMENTATION or by using or running THIS IMPLEMENTATION.

  "PATENT CHALLENGE" shall mean a challenge to the validity,
  patentability, enforceability and/or non-infringement of any of the
  PATENT CLAIMS or otherwise opposing any of the PATENT CLAIMS.

  Tokutek hereby grants to you, for the term and geographical scope of
  the PATENT CLAIMS, a non-exclusive, no-charge, royalty-free,
  irrevocable (except as stated in this section) patent license to
  make, have made, use, offer to sell, sell, import, transfer, and
  otherwise run, modify, and propagate the contents of THIS
  IMPLEMENTATION, where such license applies only to the PATENT
  CLAIMS.  This grant does not include claims that would be infringed
  only as a consequence of further modifications of THIS
  IMPLEMENTATION.  If you or your agent or licensee institute or order
  or agree to the institution of patent litigation against any entity
  (including a cross-claim or counterclaim in a lawsuit) alleging that
  THIS IMPLEMENTATION constitutes direct or contributory patent
  infringement, or inducement of patent infringement, then any rights
  granted to you under this License shall terminate as of the date
  such litigation is filed.  If you or your agent or exclusive
  licensee institute or order or agree to the institution of a PATENT
  CHALLENGE, then Tokutek may terminate any rights granted to you
  under this License.
*/

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."



#include "test.h"

static void
test_create (void) {
    message_buffer msg_buffer;
    msg_buffer.create();
    msg_buffer.destroy();
}

static void
test_enqueue(int n) {
    int r;
    message_buffer msg_buffer;
    MSN startmsn = ZERO_MSN;

    msg_buffer.create();
    char *thekey = 0; int thekeylen;
    char *theval = 0; int thevallen;

    // this was a function but icc cant handle it    
#define buildkey(len) { \
        thekeylen = len+1; \
        XREALLOC_N(thekeylen, thekey); \
        memset(thekey, len, thekeylen); \
    }

#define buildval(len) { \
        thevallen = len+2; \
        XREALLOC_N(thevallen, theval); \
        memset(theval, ~len, thevallen); \
    }

    for (int i=0; i<n; i++) {
        buildkey(i);
        buildval(i);
        XIDS xids;
        if (i==0)
            xids = xids_get_root_xids();
        else {
            r = xids_create_child(xids_get_root_xids(), &xids, (TXNID)i);
            assert(r==0);
        }
        MSN msn = next_dummymsn();
        if (startmsn.msn == ZERO_MSN.msn)
            startmsn = msn;
        enum ft_msg_type type = (enum ft_msg_type) i;
        DBT k, v;
        FT_MSG_S msg = {
            type, msn, xids, .u = { .id = { toku_fill_dbt(&k, thekey, thekeylen), toku_fill_dbt(&v, theval, thevallen) } }
        };
        msg_buffer.enqueue(&msg, true, nullptr);
        xids_destroy(&xids);
    }

    struct checkit_fn {
        MSN startmsn;
        int verbose;
        int i;
        checkit_fn(MSN smsn, bool v)
            : startmsn(smsn), verbose(v), i(0) {
        }
        int operator()(FT_MSG msg, bool UU(is_fresh)) {
            char *thekey = nullptr;
            int thekeylen = 0;
            char *theval = nullptr;
            int thevallen = 0;
            buildkey(i);
            buildval(i);

            MSN msn = msg->msn;
            enum ft_msg_type type = ft_msg_get_type(msg);
            if (verbose) printf("checkit %d %d %" PRIu64 "\n", i, type, msn.msn);
            assert(msn.msn == startmsn.msn + i);
            assert((int) ft_msg_get_keylen(msg) == thekeylen); assert(memcmp(ft_msg_get_key(msg), thekey, ft_msg_get_keylen(msg)) == 0);
            assert((int) ft_msg_get_vallen(msg) == thevallen); assert(memcmp(ft_msg_get_val(msg), theval, ft_msg_get_vallen(msg)) == 0);
            assert(i % 256 == (int)type);
            assert((TXNID)i==xids_get_innermost_xid(ft_msg_get_xids(msg)));
            toku_free(thekey);
            toku_free(theval);
            i += 1;
            return 0;
        }
    } checkit(startmsn, verbose);
    msg_buffer.iterate(checkit);
    assert(checkit.i == n);

    if (thekey) toku_free(thekey);
    if (theval) toku_free(theval);

    msg_buffer.destroy();
}

int
test_main(int argc, const char *argv[]) {
    default_parse_args(argc, argv);
    initialize_dummymsn();
    test_create();
    test_enqueue(4);
    test_enqueue(512);
    
    return 0;
}
