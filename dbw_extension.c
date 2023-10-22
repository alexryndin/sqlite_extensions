/* Add your header comment here */
#include <assert.h>
#include <ctype.h>
#include <dbg.h>
#include <md4c-html.h>
#include <sds.h>
#include <sqlite3ext.h> /* Do not use <sqlite3.h>! */
#include <stdlib.h>
SQLITE_EXTENSION_INIT1

/* Insert your extension code here */

#ifdef _WIN32
__declspec(dllexport)
#endif

    static void string_append(const MD_CHAR *ptr, MD_SIZE size, void *str) {
  CHECK(str != NULL, "Null str");
  char **_str = str;
  CHECK(
      (*_str = sdscatlen(*_str, ptr, size)) != NULL,
      "Couldn't append to string");
error:
  return;
}

static int check_name(char *name) {
  for (int i = 0; name[i] != '\0'; i++) {
    if (!isalnum(name[i]) && name[i] != '_' && name[i] != '-') {
      return 0;
    }
  }
  return 1;
}

static void
sqlite3_path_ascend(sqlite3_context *ctx, int argc, sqlite3_value **argv) {
  if (argc != 3 || sqlite3_value_type(argv[0]) != SQLITE_INTEGER ||
      sqlite3_value_type(argv[1]) != SQLITE_TEXT ||
      sqlite3_value_type(argv[2]) != SQLITE_TEXT) {
    sqlite3_result_error(ctx, "Invalid arguments", -1);
    return;
  }

  char *q = NULL;
  char *res = NULL;
  int errflg = 0;
  sqlite3_stmt *stmt = NULL;
  sqlite3 *db = sqlite3_context_db_handle(ctx);

  int id = sqlite3_value_int(argv[0]);
  char *table_name = (char *)sqlite3_value_text(argv[1]);
  if (!strcmp(table_name, "")) {
    table_name = "snippets";
  }
  char *dirs_table_name = (char *)sqlite3_value_text(argv[2]);
  if (!strcmp(dirs_table_name, "")) {
    dirs_table_name = "dirs";
  }

  if (!(check_name(table_name) && check_name(dirs_table_name))) {
    sqlite3_result_error(ctx, "Invalid arguments", -1);
  }

  q = sdsnew("");
  if (q == NULL) {
    sqlite3_result_error_nomem(ctx);
    goto exit;
  }
  q = sdscatprintf(
      q,
      "WITH RECURSIVE "
      "ascend(x, id, name, parent_id) AS ( "
      "select 1, id, name, parent_id from %1$s where id = (select dir from "
      "%2$s where id = ?) "
      "UNION "
      "SELECT x+1, %1$s.id, %1$s.name, %1$s.parent_id from %1$s, ascend "
      "where ascend.parent_id = %1$s.id and ascend.id != ascend.parent_id "
      "limit 255 "
      ") "
      "select IIF(count(*) > 1, substr(group_concat(name, '/'),5), '/') from "
      "(select name from ascend "
      "order by x desc);",
      dirs_table_name,
      table_name);
  if (q == NULL) {
    sqlite3_result_error_nomem(ctx);
    goto exit;
  }
  CHECK(
      sqlite3_prepare_v2(db, q, sdslen(q) + 1, &stmt, NULL) == SQLITE_OK,
      "Couldn't prepare statement: %s",
      sqlite3_errmsg(db));
  CHECK(
      sqlite3_bind_int64(stmt, 1, id) == SQLITE_OK,
      "Couldn't bind parameter to statement");
  int err = sqlite3_step(stmt);
  switch (err) {
  case SQLITE_ROW:
    res = sdscat(sdsempty(), sqlite3_column_text(stmt, 0));
    CHECK(res != NULL, "Couldn't create string");
    break;
  case SQLITE_DONE:
  default:
    LOG_ERR("Couldn't get row from table: %s", sqlite3_errmsg(db));
    goto error;
  }

  errflg = 0;
exit:
  if (q != NULL) {
    sdsfree(q);
  }
  if (stmt != NULL) {
    sqlite3_finalize(stmt);
  }
  if (!errflg) {
    sqlite3_result_text(ctx, res, sdslen(res), (void (*)(void *))sdsfree);
  }
  return;
error:
  errflg = 1;
  sqlite3_result_error(ctx, "db error", -1);
  goto exit;
}

static void
sqlite3_path_descend(sqlite3_context *ctx, int argc, sqlite3_value **argv) {
  if (argc != 5 || sqlite3_value_type(argv[0]) != SQLITE_TEXT ||
      sqlite3_value_type(argv[1]) != SQLITE_TEXT ||
      sqlite3_value_type(argv[2]) != SQLITE_TEXT ||
      sqlite3_value_type(argv[3]) != SQLITE_TEXT ||
      sqlite3_value_type(argv[4]) != SQLITE_TEXT) {
    sqlite3_result_error(ctx, "Invalid arguments", -1);
    return;
  }

  char **s_path = NULL;
  char *q = NULL;
  int ret = 1;
  int errflg = 0;
  sqlite3 *db = sqlite3_context_db_handle(ctx);
  sqlite3_stmt *stmt = NULL;

  char *path = (char *)sqlite3_value_text(argv[0]);
  if (!strcmp(path, "") || !strcmp(path, "/")) {
    sqlite3_result_int64(ctx, ret);
    goto exit;
  }
  char *table_name = (char *)sqlite3_value_text(argv[1]);
  if (!strcmp(table_name, "")) {
    table_name = "snippets";
  }
  char *column_name = (char *)sqlite3_value_text(argv[2]);
  if (!strcmp(column_name, "")) {
    column_name = "dirs";
  }
  char *id_column_name = (char *)sqlite3_value_text(argv[3]);
  if (!strcmp(id_column_name, "")) {
    id_column_name = "id";
  }
  char *parent_id_column_name = (char *)sqlite3_value_text(argv[4]);
  if (!strcmp(parent_id_column_name, "")) {
    parent_id_column_name = "parent_id";
  }

  if (!(check_name(table_name) && check_name(column_name) &&
        check_name(id_column_name) && check_name(parent_id_column_name))) {
    sqlite3_result_error(ctx, "Invalid arguments", -1);
  }

  q = sdsnew("");
  if (q == NULL) {
    sqlite3_result_error_nomem(ctx);
    goto exit;
  }
  q = sdscatprintf(
      q,
      "select a.%s from %s as a"
      " join %s as b on b.%s == a.%s"
      " where b.%s = ? and a.name = ?",
      id_column_name,
      table_name,
      table_name,
      id_column_name,
      parent_id_column_name,
      id_column_name);
  if (q == NULL) {
    sqlite3_result_error_nomem(ctx);
    goto exit;
  }
  int cnt;
  s_path = sdssplitlen(path, strlen(path), "/", 1, &cnt);
  if (s_path == NULL) {
    sqlite3_result_error_nomem(ctx);
    goto exit;
  }
  for (int i = 1; i < cnt; i++) {
    if (sdslen(s_path[i]) == 0) {
      continue;
    }
    CHECK(
        sqlite3_prepare_v2(db, q, sdslen(q) + 1, &stmt, NULL) == SQLITE_OK,
        "Couldn't prepare statement: %s",
        sqlite3_errmsg(db));
    CHECK(
        sqlite3_bind_int64(stmt, 1, ret) == SQLITE_OK,
        "Couldn't bind parameter to statement");
    CHECK(
        sqlite3_bind_text(stmt, 2, s_path[i], sdslen(s_path[i]), NULL) ==
            SQLITE_OK,
        "Couldn't bind parameter to statement");
    int err = sqlite3_step(stmt);
    switch (err) {
    case SQLITE_DONE:
      ret = -1;
      goto exit;
      break;
    case SQLITE_ROW:
      ret = sqlite3_column_int64(stmt, 0);
      break;
    default:
      LOG_ERR("Couldn't get row from table: %s", sqlite3_errmsg(db));
      goto error;
    }
  }

exit:
  if (q != NULL) {
    sdsfree(q);
  }
  if (s_path != NULL) {
    sdsfreesplitres(s_path, cnt);
  }
  if (stmt != NULL) {
    sqlite3_finalize(stmt);
  }
  if (!errflg) {
    sqlite3_result_int(ctx, ret);
  }
  return;
error:
  sqlite3_result_error(ctx, "db error", -1);
  errflg = 1;
  goto exit;
}

static void
sqlite3_md2html(sqlite3_context *ctx, int argc, sqlite3_value **argv) {
  assert(argc == 1);
  char *ret = sdsnew("");
  if (ret == NULL) {
    sqlite3_result_error_nomem(ctx);
    return;
  }
  const unsigned char *text = sqlite3_value_text(argv[0]);
  if (text == NULL) {
    sqlite3_result_error_nomem(ctx);
    sdsfree(ret);
    return;
  }
  int err = 0;
  err = md_html(
      (const MD_CHAR *)text,
      strlen((const char *)text),
      string_append,
      &ret,
      MD_FLAG_STRIKETHROUGH | MD_FLAG_TASKLISTS,
      0);
  if (err != 0) {
    sqlite3_result_error_nomem(ctx);
    sdsfree(ret);
    return;
  }
  const int length = (int)sdslen(ret);
  sqlite3_result_text(ctx, ret, length, (void (*)(void *))sdsfree);
}

static void
sqlite3_html_escape(sqlite3_context *ctx, int argc, sqlite3_value **argv) {
  assert(argc == 1);
  char *ret = sdsnew("");
  if (ret == NULL) {
    sqlite3_result_error_nomem(ctx);
    return;
  }
  const unsigned char *text = sqlite3_value_text(argv[0]);
  if (text == NULL) {
    sqlite3_result_error_nomem(ctx);
    sdsfree(ret);
    return;
  }
  for (size_t i = 0; text[i] != '\0'; i++) {
    switch (text[i]) {
    case '"':
      ret = sdscat(ret, "&quot;");
      break;
    case '&':
      ret = sdscat(ret, "&amp;");
      break;
    case '\'':
      ret = sdscat(ret, "&#39;");
      break;
    case '<':
      ret = sdscat(ret, "&lt;");
      break;
    case '>':
      ret = sdscat(ret, "&gt;");
      break;
    default:
      ret = sdscatlen(ret, &text[i], 1);
    }
    if (ret == NULL) {
      sdsfree(ret);
      sqlite3_result_error_nomem(ctx);
      return;
    }
  }
  sqlite3_result_text(ctx, ret, sdslen(ret), (void (*)(void *))sdsfree);
}

int sqlite3_dbwextension_init(
    sqlite3 *db, char **pzErrMsg, const sqlite3_api_routines *pApi) {
  int rc = SQLITE_OK;
  SQLITE_EXTENSION_INIT2(pApi);
  /* Insert here calls to
  **     sqlite3_create_function_v2(),
  **     sqlite3_create_collation_v2(),
  **     sqlite3_create_module_v2(), and/or
  **     sqlite3_vfs_register()
  ** to register the new features that your extension adds.
  */
  rc = sqlite3_create_function_v2(
      db,
      "html_escape",
      1,
      SQLITE_UTF8,
      NULL,
      sqlite3_html_escape,
      NULL,
      NULL,
      NULL);

  if (rc != SQLITE_OK) {
    goto error;
  }

  sqlite3_create_function_v2(
      db, "md2html", 1, SQLITE_UTF8, NULL, sqlite3_md2html, NULL, NULL, NULL);

  sqlite3_create_function_v2(
      db,
      "path_descend",
      5,
      SQLITE_UTF8,
      NULL,
      sqlite3_path_descend,
      NULL,
      NULL,
      NULL);

  sqlite3_create_function_v2(
      db,
      "path_ascend",
      3,
      SQLITE_UTF8,
      NULL,
      sqlite3_path_ascend,
      NULL,
      NULL,
      NULL);

error:
  return rc;
}
