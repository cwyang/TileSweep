#include <ctype.h>
#include <fcntl.h>
#include <h2o.h>
#include "hash/xxhash.h"
#include "image.h"
#include "image_db.h"
#include "tcp.h"
#include "tile_renderer.h"
#include "tilelite_config.h"
#include "tl_log.h"
#include "tl_request.h"
#include "tl_time.h"

struct tl_h2o_ctx {
  h2o_context_t super;
  void* user;
};

struct tilelite {
  h2o_accept_ctx_t accept_ctx;
  h2o_socket_t* socket;
  image_db* db;
  tile_renderer renderer;
  tl_h2o_ctx ctx;
  int fd;
  size_t index;
};

static struct {
  h2o_globalconf_t globalconf;
  tilelite_config* opt;
  size_t num_threads;
  tilelite* threads;
} conf = {0};

static void on_accept(h2o_socket_t* listener, const char* err) {
  if (err != NULL) {
    printf("Accept error: %s\n", err);
    return;
  }

  h2o_socket_t* socket = h2o_evloop_socket_accept(listener);
  if (socket == NULL) {
    return;
  }

  tilelite* tl = (tilelite*)listener->data;
  h2o_accept(&tl->accept_ctx, socket);
}

void tilelite_init(tilelite* tl, const tilelite_config* opt) {
  printf("tilelite init %p\n", tl);
  if (opt->at("rendering") == "1") {
    tile_renderer_init(&tl->renderer, opt->at("mapnik_xml").c_str(),
                       opt->at("plugins").c_str(), opt->at("fonts").c_str());
  }

  h2o_context_init(&tl->ctx.super, h2o_evloop_create(), &conf.globalconf);
  tl->ctx.user = tl;
  tl->accept_ctx.ctx = &tl->ctx.super;
  tl->accept_ctx.hosts = conf.globalconf.hosts;

  tl->socket = h2o_evloop_socket_create(tl->ctx.super.loop, tl->fd,
                                        H2O_SOCKET_FLAG_DONT_READ);
  tl->socket->data = tl;
  h2o_socket_read_start(tl->socket, on_accept);
}

static int strntoi(const char* s, size_t len) {
  int n = 0;

  for (size_t i = 0; i < len; i++) {
    n = n * 10 + s[i] - '0';
  }

  return n;
}

static tl_tile parse_tile(const char* s, size_t len) {
  size_t begin = 0;
  size_t end = 0;
  int part = 0;
  int parsing_num = 0;
  int params[5] = {-1};

  for (size_t i = 0; i < len; i++) {
    if (isdigit(s[i])) {
      if (!parsing_num) {
        begin = i;
        parsing_num = 1;
      }
      end = i;
    } else {
      if (parsing_num) {
        params[part++] = int(strntoi(&s[begin], end - begin + 1));
        parsing_num = 0;
      }
    }

    if (i == len - 1 && parsing_num) {
      params[part] = int(strntoi(&s[begin], end - begin + 1));
    }
  }

  tl_tile t;
  t.w = params[0];
  t.h = params[1];
  t.x = params[2];
  t.y = params[3];
  t.z = params[4];

  return t;
}

static h2o_pathconf_t* RegisterHandler(h2o_hostconf_t* conf, const char* path,
                                       int (*onReq)(h2o_handler_t*,
                                                    h2o_req_t*)) {
  h2o_pathconf_t* pathConf = h2o_config_register_path(conf, path, 0);
  h2o_handler_t* handler = h2o_create_handler(pathConf, sizeof(*handler));
  handler->on_req = onReq;

  return pathConf;
}

static int ServeTile(h2o_handler_t*, h2o_req_t* req) {
  tl_h2o_ctx* x = (tl_h2o_ctx*)req->conn->ctx;
  tilelite* tl = (tilelite*)x->user;
  int64_t req_start = tl_usec_now();
  h2o_generator_t gen = {NULL, NULL};
  req->res.status = 200;
  req->res.reason = "OK";
  tl_tile t = parse_tile(req->path.base, req->path.len);

  uint64_t pos_hash = t.hash();
  image img;

  bool existing = image_db_fetch(tl->db, pos_hash, t.w, t.h, &img);
  if (!existing) {
    if (render_tile(&tl->renderer, &t, &img)) {
      uint64_t image_hash = XXH64(img.data, img.len, 0);
      image_db_add_image(tl->db, &img, image_hash);
      image_db_add_position(tl->db, pos_hash, image_hash);
    }
  }

  h2o_start_response(req, &gen);
  if (img.len > 0) {
    h2o_add_header(&req->pool, &req->res.headers, H2O_TOKEN_CONTENT_TYPE,
                   H2O_STRLIT("image/png"));
    h2o_iovec_t body;
    body.base = (char*)img.data;
    body.len = img.len;
    h2o_send(req, &body, 1, 1);

    free(img.data);
  } else {
    h2o_send(req, &req->entity, 1, 1);
  }

  int64_t req_time = tl_usec_now() - req_start;
  if (req_time > 1000) {
    tl_log("[%lu] [%d, %d, %d, %d, %d] (%d bytes) | %.2f ms", tl->index, t.w,
           t.h, t.z, t.x, t.y, img.len, double(req_time) / 1000.0);
  } else {
    tl_log("[%lu] [%d, %d, %d, %d, %d] (%d bytes) | %ld us", tl->index, t.w,
           t.h, t.z, t.x, t.y, img.len, req_time);
  }

  return 0;
}

void* run_loop(void* arg) {
  size_t idx = size_t(arg);
  tilelite* tl = &conf.threads[idx];
  tl->index = idx;
  tilelite_init(tl, conf.opt);

  printf("TL idx: %lu %p\n", tl->index, tl);

  while (h2o_evloop_run(tl->ctx.super.loop) == 0) {
  }

  return NULL;
}

int main(int argc, char** argv) {
  tilelite_config opt = parse_args(argc, argv);
  conf.opt = &opt;
  conf.num_threads = 4;
  conf.threads = (tilelite*)calloc(conf.num_threads, sizeof(tilelite));
  h2o_config_init(&conf.globalconf);

  h2o_hostconf_t* hostconf = h2o_config_register_host(
      &conf.globalconf, h2o_iovec_init(H2O_STRLIT("default")), 65535);
  RegisterHandler(hostconf, "/", ServeTile);

  for (size_t i = 0; i < conf.num_threads; i++) {
    tilelite* tl = &conf.threads[i];
    tl->db = image_db_open(opt.at("tile_db").c_str());
  }

  int sock_fd = bind_tcp("127.0.0.1", "9567");
  if (sock_fd == -1) {
    return 1;
  }

  for (size_t i = 0; i < conf.num_threads; i++) {
    tilelite* tl = &conf.threads[i];
    tl->fd = sock_fd;
  }

  for (size_t i = 1; i < conf.num_threads; i++) {
    pthread_t tid;
    h2o_multithread_create_thread(&tid, NULL, &run_loop, (void*)i);
  }

  run_loop((void*)0);

  return 0;
}