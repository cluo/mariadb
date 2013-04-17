/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."
#include "test.h"

#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>
#include <memory.h>
#include <errno.h>
#include <sys/stat.h>
#include <db.h>


static void
cursor_expect (DBC *cursor, int k, int v, int op) {
    DBT key, val;
    int r = cursor->c_get(cursor, dbt_init_malloc(&key), dbt_init_malloc(&val), op);
    assert(r == 0);
    assert(key.size == sizeof k);
    int kk;
    memcpy(&kk, key.data, key.size);
    assert(val.size == sizeof v);
    int vv;
    memcpy(&vv, val.data, val.size);
    if (kk != k || vv != v) printf("expect key %u got %u - %u %u\n", (uint32_t)htonl(k), (uint32_t)htonl(kk), (uint32_t)htonl(v), (uint32_t)htonl(vv));
    assert(kk == k);
    assert(vv == v);

    toku_free(key.data);
    toku_free(val.data);
}


/* generate a multi-level tree and delete all entries with a cursor
   verify that the pivot flags are toggled (currently by inspection) */

static void
test_cursor_delete (int dup_mode) {
    if (verbose) printf("test_cursor_delete:%d\n", dup_mode);

    int pagesize = 4096;
    int elementsize = 32;
    int npp = pagesize/elementsize;
    int n = 16*npp; /* build a 2 level tree */

    DB_TXN * const null_txn = 0;
    const char * const fname = "test.cursor.delete.brt";
    int r;

    r = system("rm -rf " ENVDIR); assert(r == 0);
    r = toku_os_mkdir(ENVDIR, S_IRWXU|S_IRWXG|S_IRWXO); assert(r == 0);
    
    /* create the dup database file */
    DB_ENV *env;
    r = db_env_create(&env, 0); assert(r == 0);
#ifdef USE_TDB
    r = env->set_redzone(env, 0);                                 CKERR(r);
#endif
    r = env->open(env, ENVDIR, DB_CREATE+DB_PRIVATE+DB_INIT_MPOOL, 0); assert(r == 0);

    DB *db;
    r = db_create(&db, env, 0); assert(r == 0);
    db->set_errfile(db,0); // Turn off those annoying errors
    r = db->set_flags(db, dup_mode); assert(r == 0);
    r = db->set_pagesize(db, pagesize); assert(r == 0);
    r = db->open(db, null_txn, fname, "main", DB_BTREE, DB_CREATE, 0666); assert(r == 0);

    int i;
    for (i=0; i<n; i++) {
        int k = htonl(i);
        int v = htonl(i);
        DBT key, val;
        r = db->put(db, null_txn, dbt_init(&key, &k, sizeof k), dbt_init(&val, &v, sizeof v), DB_YESOVERWRITE); assert(r == 0);
    }

    /* verify the sort order with a cursor */
    DBC *cursor;
    r = db->cursor(db, null_txn, &cursor, 0); assert(r == 0);

    for (i=0; i<n; i++) {
        cursor_expect(cursor, htonl(i), htonl(i), DB_NEXT); 
        
        r = cursor->c_del(cursor, 0); assert(r == 0);
     }

    r = cursor->c_close(cursor); assert(r == 0);

    r = db->close(db, 0); assert(r == 0);
    r = env->close(env, 0); assert(r == 0);
}

int
test_main(int argc, char *const argv[]) {
    parse_args(argc, argv);

    test_cursor_delete(0);
#ifdef USE_BDB
    test_cursor_delete(DB_DUP);
#endif

    return 0;
}
