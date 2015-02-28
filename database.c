#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include "CuTest.h"

struct Address {
  int id;
  int set;
  char *name;
  char *email;
};

struct Database {
  int max_data;
  int max_rows;
  int count;
  struct Address *rows;
};

struct Connection {
  FILE *file;
  struct Database *db;
};

void Address_print(struct Address *addr) {
  printf("%d %s %s\n", addr->id, addr->name, addr->email);
}

void Database_close(struct Connection *conn) {
  if(conn) {
    if(conn->file) fclose(conn->file);
    if(conn->db) {
      if(conn->db->rows) {
        int i = 0;
        for(i = 0; i < conn->db->max_rows; i++) {
          if(conn->db->rows[i].name) free(conn->db->rows[i].name);
          if(conn->db->rows[i].email) free(conn->db->rows[i].email);
        }
        free(conn->db->rows);
      }
      free(conn->db);
    }
    free(conn);
  }
}

void die(struct Connection *conn, const char *message) {
  if(errno) {
    perror(message);
  } else {
    printf("ERROR: %s\n", message);
  }
  Database_close(conn);
  exit(1);
}

void Database_create(struct Connection *conn) {
  int i = 0;

  for(i = 0; i < conn->db->max_rows; i++) {
    //prototype
    struct Address addr = {.id = i, .set = 0, .name = malloc(conn->db->max_data), .email = malloc(conn->db->max_data)};
    conn->db->rows[i] = addr;
  }
}

struct Connection *Database_load(const char *filename, char mode) {
  struct Connection *conn = malloc(sizeof(struct Connection));
  if(!conn) die(NULL, "Memory error");

  //allocate db
  conn->db = malloc(sizeof(struct Database));
  if(!conn->db) die(conn, "Memory error");

  //open file
  conn->file = fopen(filename, "r+");
  if(!conn->file) die(conn, "Failed to open the file");

  //load max_data/max_rows
  fread(&conn->db->max_data, sizeof(int), 1, conn->file);
  fread(&conn->db->max_rows, sizeof(int), 1, conn->file);

  //load count
  fread(&conn->db->count, sizeof(int), 1, conn->file);

  //allocate database rows
  conn->db->rows = malloc(conn->db->max_rows * sizeof(struct Address));
  if(!conn->db->rows) die(conn, "Memory error");

  //create the empty database
  Database_create(conn);

  //load records into database
  int i;
  for(i = 0; i < conn->db->count; i++) {
    int id = 0;
    fread(&id, sizeof(int), 1, conn->file);

    fread(conn->db->rows[id].name, conn->db->max_data, 1, conn->file);
    fread(conn->db->rows[id].email, conn->db->max_data, 1, conn->file);
    conn->db->rows[id].set = 1;
  }
  return conn;
}

struct Connection *Database_init(const char *filename, const int max_data, const int max_rows) {
  struct Connection *conn = malloc(sizeof(struct Connection));
  if(!conn) die(NULL, "Memory error");

  conn->db = malloc(sizeof(struct Database));
  if(!conn->db) die(conn, "Memory error");
  conn->db->max_data = max_data;
  conn->db->max_rows = max_rows;

  conn->db->count = 0;

  conn->db->rows = malloc(max_rows * sizeof(struct Address));
  if(!conn->db->rows) die(conn, "Memory error");

  conn->file = fopen(filename, "w");
  if(!conn->file) die(conn, "Failed to open the file");
  return conn;
}


void Database_write(struct Connection *conn) {
  //set the pointer to beginning
  rewind(conn->file);

  //store max_rows and max_data
  int rc = fwrite(&conn->db->max_data, sizeof(int), 1, conn->file);
  if(rc != 1) die(conn, "Failed to write max_data to database.");

  rc = fwrite(&conn->db->max_rows, sizeof(int), 1, conn->file);
  if(rc != 1) die(conn, "Failed to write max_rows to database.");

  //store count
  fwrite(&conn->db->count, sizeof(int), 1, conn->file);

  //store data
  if(conn->db->count > 0) {
    int i;
    for(i = 0; i < conn->db->max_rows; i++){
      if(conn->db->rows[i].set) {
        fwrite(&conn->db->rows[i].id, sizeof(int), 1, conn->file);
        fwrite(conn->db->rows[i].name, conn->db->max_data, 1, conn->file);
        fwrite(conn->db->rows[i].email, conn->db->max_data, 1, conn->file);
      }
    }
  }


  //forces write of buffered data
  rc = fflush(conn->file);
  if(rc == -1) die(conn, "Cannot flush database.");
}

void Database_set(struct Connection *conn, int id, const char *name, const char *email) {
  char message[30];
  sprintf(message, "Database has maximum %d records.", conn->db->max_rows);
  if(id >= conn->db->max_rows) die(conn, message);

  struct Address *addr = &conn->db->rows[id];
  if(addr->set) die(conn, "Already set. Delete it first.");

  addr->set = 1;

  char *res = strncpy(addr->name, name, conn->db->max_data);
  addr->name[conn->db->max_data - 1] = '\0';
  if(!res) die(conn, "Name copy failed");

  res = strncpy(addr->email, email, conn->db->max_data);
  addr->email[conn->db->max_data - 1] = '\0';
  if(!res) die(conn, "Email copy failed");

  conn->db->count += 1;
}

void Database_get(struct Connection *conn, int id) {
  struct Address *addr = &conn->db->rows[id];

  if(addr->set) {
    Address_print(addr);
  } else {
    die(conn, "ID is not set");
  }
}

void Database_delete(struct Connection *conn, int id) {
  struct Address addr = {.id = id, .set = 0, .name = malloc(conn->db->max_data), .email = malloc(conn->db->max_data)};
  if(id <= conn->db->max_rows) {
    free(conn->db->rows[id].name);
    free(conn->db->rows[id].email);
    conn->db->rows[id] = addr;
  }
}

void Database_list(struct Connection *conn) {
  int i = 0;
  struct Database *db = conn->db;

  for(i = 0; i < conn->db->max_rows; i++) {
    struct Address *cur = &db->rows[i];

    if(cur->set) {
      Address_print(cur);
    }
  }
}

/*
void TestOpen(CuTest *tc) {
  struct Connection *conn = Database_open("test.db", 'c', 5, 10);
  CuAssertPtrNotNull(tc, conn);
  CuAssertPtrNotNull(tc, conn->db);
  CuAssertPtrNotNull(tc, conn->db->rows);
  CuAssertPtrNotNull(tc, conn->file);
  CuAssertIntEquals(tc, 5, conn->db->max_data);
  CuAssertIntEquals(tc, 10, conn->db->max_rows);
}

void TestCreate(CuTest *tc) {
  struct Connection *conn = Database_open("test.db", 'c', 10, 2);
  Database_create(conn);
  CuAssertPtrNotNull(tc, &conn->db->rows[0]);
  CuAssertPtrNotNull(tc, &conn->db->rows[1]);
  CuAssertIntEquals(tc, 0, conn->db->rows[0].id);
  CuAssertIntEquals(tc, 1, conn->db->rows[1].id);
  CuAssertIntEquals(tc, 0, conn->db->rows[0].set);
  CuAssertIntEquals(tc, 0, conn->db->rows[1].set);
  Database_close(conn);
}

void TestSet(CuTest *tc) {
  struct Connection *conn = Database_open("test.db", 'c', 10, 2);
  Database_create(conn);
  Database_set(conn, 0, "Jiayi", "jy@z.de");
  Database_set(conn, 1, "Maximilliana", "max@mamamamax.de");
  CuAssertStrEquals(tc, "Jiayi", conn->db->rows[0].name);
  CuAssertStrEquals(tc, "jy@z.de", conn->db->rows[0].email);
  CuAssertStrEquals(tc, "Maximilli", conn->db->rows[1].name);
  CuAssertStrEquals(tc, "max@mamam", conn->db->rows[1].email);
  Database_close(conn);
}

void TestDelete(CuTest *tc) {
  struct Connection *conn = Database_open("test.db", 'c', 10, 2);
  Database_create(conn);
  Database_set(conn, 0, "Jiayi", "jy@z.de");
  CuAssertIntEquals(tc, 1, conn->db->rows[0].set);
  Database_delete(conn, 0);
  CuAssertIntEquals(tc, 0, conn->db->rows[0].set);
}

CuSuite *GetSuite() {
  CuSuite *suite = CuSuiteNew();
  SUITE_ADD_TEST(suite, TestOpen);
  SUITE_ADD_TEST(suite, TestCreate);
  SUITE_ADD_TEST(suite, TestSet);
  SUITE_ADD_TEST(suite, TestDelete);
  return suite;
}
*/

int main(int argc, char *argv[]) {
  if(argc < 3) die(NULL, "USAGE: ex17 <dbfile> <action> [action params]");
  
  char *filename = argv[1];
  char action = argv[2][0];
  int id, max_data, max_rows;
  struct Connection *conn;

  switch(action) {
    case 'c':
      if(argc != 5) die(conn, "Need max_data and max_rows to create database");
      max_data = atoi(argv[3]);
      max_rows = atoi(argv[4]);
      conn = Database_init(filename, max_data, max_rows);
      Database_write(conn);
      break;
    case 'g':
      if(argc != 4) die(conn, "Need an id to get");
      id = atoi(argv[3]);
      conn = Database_load(filename, action);
      Database_get(conn, id);
      break;
    case 's':
      if(argc != 6) die(conn, "Need id, name, email to set");
      id = atoi(argv[3]);
      conn = Database_load(filename, action);
      Database_set(conn, id, argv[4], argv[5]);
      Database_write(conn);
      break;
    case 'd':
      if(argc != 4) die(conn, "Need id to delete");
      id = atoi(argv[3]);
      conn = Database_load(filename, action);
      Database_delete(conn, id);
      Database_write(conn);
      break;
    case 'l':
      conn = Database_load(filename, action);
      Database_list(conn);
      break;
    default:
       die(NULL, "Invalid action, only: c=create, g=get, s=set, d=del, l=list");
  }
  if(conn) Database_close(conn);

  return 0;
}
