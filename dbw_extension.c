/* Add your header comment here */
#include <assert.h>
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

error:
  return rc;
}
