
#include <stdio.h>
#include <string>
#include <limits.h>

#include <pjlib.h>
#include <pjlib-util.h>
#include <pjnath.h>
#include <string.h>
#include <json/json.h>
#include <mstester.h>

//#include <android/log.h> // LOGD, LOGE ...

//#define DEBUG_RELAY

#define dbg(...) PJ_LOG(3, (__FILE__, __VA_ARGS__))
#define dbgi(...) PJ_LOG(3, (obj->obj_name, __VA_ARGS__))
#define dbgw(...) PJ_LOG(2, (obj->obj_name, __VA_ARGS__))
#define dbge(...) PJ_LOG(1, (obj->obj_name, __VA_ARGS__))









#define MAX_IP_SIZE 64

#define PKT_TYPE_CTRL			0xfc
#define CTRL_EVENT_TEST_ECHO		101


#define KEEP_ALIVE_INTERVAL 300
#define MAX_COMPONENT_COUNT 4
#define MAX_CANDIDATE_COUNT 8


struct unit_context{
	int pj_inited;
	int pjlib_inited;
	int pjnath_inited;
	int pjlog_installed;
	FILE *log_fhnd;
    
    pj_caching_pool cp;
    int cp_inited;
    pj_pool_t * pool;

};


class mstester_config{
public:
	std::string peer_host;
	int peer_port;
    pj_uint32_t pkt_size;
    pj_uint32_t bitrate;  // bps
    pj_uint32_t test_seconds;
};


typedef struct{
    pj_sock_t sock_fd;
    pj_sockaddr bound_addr;
    pj_activesock_t *active_sock;
    pj_ioqueue_op_key_t send_key;
}pjsock;


typedef enum unit_state
{
    US_READY = 0,
    
    US_RUNNING,
    US_STOPPING,
    US_FINISH = 99,
    
    US_MAX
    
} unit_state;



typedef struct
{
    pj_uint32_t last;
    pj_uint32_t min;
    pj_uint32_t max;
    pj_uint32_t aver;
    pj_uint32_t sum;
    pj_uint32_t count;
}statistics_item;


void statics_item_init(statistics_item * item)
{
    pj_bzero(item, sizeof(statistics_item));
    item->min = UINT_MAX;
    item->max = 0;
    item->aver = 0;
    item->last = 0;
}


void statics_item_update(statistics_item * item, pj_uint32_t v)
{
    if(v > item->max) item->max = v;
    if(v < item->min) item->min = v;
    item->sum += v;
    item->count++;
    item->aver = item->sum/item->count;
    item->last = v;
}


void statics_item_update_child(statistics_item * item, statistics_item * child)
{
    if(child->max > item->max) item->max = child->max;
    if(child->min < item->min) item->min = child->min;
    
    item->sum += child->sum;
    item->count += child->count;
    
    //item->sum += child->aver;
    //item->count += 1;
    
    if(item->count > 0)
    {
        item->aver = item->sum/item->count;
    }
    else
    {
        item->aver = item->sum;
    }
    
}


struct mstester_st{
    char		obj_name[PJ_MAX_OBJ_NAME];
    
	mstester_config * cfg;

    // ice instance
    pj_caching_pool cp;
    int cp_inited;
    pj_pool_t *ice_pool;
    char err_str[1024];

    pj_grp_lock_t * grp_lock;
    pj_timer_heap_t	*timer_heap;
    pj_ioqueue_t	*ioqueue;

    // callback thread.
    pj_thread_t *ice_thread;
    char thread_quit_flag;

    std::string * status_content;
    
    pj_timer_entry	 timer;
    
    pj_sockaddr  peer_addr;
    
    pj_thread_t * send_thread;
    pj_bool_t send_thread_stop_req;
    
    pjsock sock;
    unit_state state;
    pj_uint8_t pkt[1024];
    pj_uint32_t pkt_len;
    pj_uint32_t pkt_seq;
    pj_uint32_t sessionId;
    pj_timestamp kick_time;
    pj_timestamp stopping_time;
    
    
    // statistics
    pj_uint32_t sending_pkts;
    pj_uint64_t sending_bytes;
    
    pj_uint32_t sent_pkts;
    pj_uint64_t sent_bytes;
    
    pj_uint32_t recv_pkts;
    pj_uint64_t recv_bytes;
    pj_uint32_t expect_recv_seq;
    pj_uint32_t seq_lost_count;
    pj_uint32_t disorder_count;
    
    statistics_item st_delay;
    
    pj_timestamp check_time;
    pj_uint64_t send_bps;
    pj_uint64_t last_sent_bytes;
    pj_uint64_t recv_bps;
    pj_uint64_t last_recv_bytes;
    
    pj_uint32_t err_pkts;
};



//static struct unit_context g_unit_stor;
//static struct unit_context * g_unit = 0;

static int _worker_thread_for_pj(void *arg);
static pj_status_t handle_events(mstester_t obj, unsigned max_msec, unsigned *p_count);
//static void unit_exit();


#define     be_get_u16(xptr)   ((pj_uint16_t) ( ((xptr)[0] << 8) | ((xptr)[1] << 0) ))     /* by simon */
#define     be_set_u16(xval, xptr)  \
do{\
(xptr)[0] = (pj_uint8_t) (((xval) >> 8) & 0x000000FF);\
(xptr)[1] = (pj_uint8_t) (((xval) >> 0) & 0x000000FF);\
}while(0)

#define     be_get_u32(xptr)   ((pj_uint32_t) ((((pj_uint32_t)(xptr)[0]) << 24) | ((xptr)[1] << 16) | ((xptr)[2] << 8) | ((xptr)[3] << 0)))        /* by simon */
#define     be_set_u32(xval, xptr)  \
do{\
(xptr)[0] = (pj_uint8_t) (((xval) >> 24) & 0x000000FF);\
(xptr)[1] = (pj_uint8_t) (((xval) >> 16) & 0x000000FF);\
(xptr)[2] = (pj_uint8_t) (((xval) >> 8) & 0x000000FF);\
(xptr)[3] = (pj_uint8_t) (((xval) >> 0) & 0x000000FF);\
}while(0)



//PJ_DEF(pj_status_t) pj_thread_auto_register(void)
static pj_status_t _auto_register_pj_thread()
{
    pj_status_t rc;
    
    if(!pj_thread_is_registered())
    {
        pj_thread_desc *p_thread_desc;
        pj_thread_t* thread_ptr;
        p_thread_desc = (pj_thread_desc *) calloc(1, sizeof(pj_thread_desc));
        rc = pj_thread_register("ethrd_%p", *p_thread_desc, &thread_ptr);
        
//        char err_str[128];
//        pj_str_t es = pj_strerror(rc, err_str, sizeof(err_str));
//        __android_log_print(ANDROID_LOG_INFO, "mst", "register result: rc=%d (%s)", rc, es.ptr);
    }
    else
    {
        rc = PJ_SUCCESS;
    }
    return rc;
}

void check_register_pj_thread()
{
    _auto_register_pj_thread();
}


static void log_func_4pj(int level, const char *data, int len) {

//    register_eice_thread(g_unit->pool);
    
//    if(!g_unit) return;

    //pj_log_write(level, data, len);
    printf("%s", data);
}


#define dbgraw(x) printf(x "\n")

//static int unit_init()
//{
//    
//    int ret = -1;
//    dbgraw("unit_init");
//    
//	if (g_unit) {
//		ret = 0;
//		dbgraw("unit is already initialized, return ok directly.");
//		return 0;
//	}
//
//   
//    
//	g_unit = &g_unit_stor;
//	memset(g_unit, 0, sizeof(struct unit_context));
//    dbgraw("memset OK");
//
//
//    do {
//        
//        ret = pj_init();
//        if (ret != PJ_SUCCESS) {
//            dbgraw("pj_init failure ");
//            break;
//        }
//        g_unit->pj_inited = 1;
//        dbgraw("pj_init OK");
//        
//        ret = _auto_register_pj_thread();
//        if (ret != PJ_SUCCESS) {
//            dbgraw("auto reg pj thread failure ");
//            break;
//        }
//        dbgraw("auto reg pj thread OK");
//        
//        // create pool factory
//        pj_caching_pool_init(&g_unit->cp, NULL, 0);
//        g_unit->cp_inited = 1;
//        dbgraw("pj_caching_pool_init OK");
//        
//        ret = _auto_register_pj_thread();
//        if (ret != PJ_SUCCESS) {
//            dbgraw("auto reg pj thread 2 failure ");
//            break;
//        }
//        dbgraw("auto reg pj thread 2 OK");
//        
//        /* Create application memory pool */
//        g_unit->pool = pj_pool_create(&g_unit->cp.factory, "eice_global_pool",
//                                      512, 512, NULL);
//        dbgraw("pj_pool_create OK");
//        
//        pj_log_set_log_func(&log_func_4pj);
//        dbgraw("pj_log_set_level OK");
//        
//        pj_log_set_level(PJ_LOG_MAX_LEVEL);
//        dbgraw("pj_log_set_level OK");
//        
//        
//        ret = pjlib_util_init();
//        if (ret != PJ_SUCCESS) {
//            dbg("pjlib_util_init failure, ret=%d", ret);
//            break;
//        }
//        g_unit->pjlib_inited = 1;
//        dbgraw("pjlib_util_init OK");
//        
//        ret = pjnath_init();
//        if (ret != PJ_SUCCESS) {
//            dbg("pjnath_init failure, ret=%d", ret);
//            break;
//        }
//        g_unit->pjnath_inited = 1;
//        dbgraw("pjnath_init OK");
//        
//
//        
//        ret = PJ_SUCCESS;
//        dbgraw("eice init ok");
//
//    } while (0);
//
//    if(ret != PJ_SUCCESS){
//    	unit_exit();
//    }
//    return ret;
//}
//
//static void unit_exit() {
//
//	if(!g_unit) return;
//    
////    if(g_unit->pool){
////        register_eice_thread(g_unit->pool);
////    }
//    
//    if(g_unit->cp_inited){
//        pj_caching_pool_destroy(&g_unit->cp);
//        g_unit->cp_inited = 0;
//    }
//    
//
//	if(g_unit->pj_inited){
//		pj_shutdown();
//		g_unit->pj_inited = 0;
//	}
//
//	dbgraw("unit exit ok");
//	g_unit = 0;
//}



static std::string _json_get_string(Json_em::Value& request, const char * name,
		const std::string& default_value) {
	if (request[name].isNull()) {
		dbg("name %s NOT found in json!!!", name);
		return default_value;
	}

	if (!request[name].isString()) {
		dbg("name %s is NOT string in json!!!", name);
		return default_value;
	}
	return request[name].asString();
}

static int _json_get_int(Json_em::Value& request, const char * name,
		unsigned int default_value) {
	if (request[name].isNull()) {
		dbg("name %s NOT found in json!!!", name);
		return default_value;
	}

	if (!request[name].isUInt()) {
		dbg("name %s is NOT UInt in json!!!", name);
		return default_value;
	}
	return request[name].asInt();
}

static
const Json_em::Value& _json_get_array(Json_em::Value& request, const char * name, const Json_em::Value& default_value)
{
	if(request[name].isNull())
	{
		dbg("name %s NOT found in json!!!", name);
		return default_value;
	}

	if(!request[name].isArray())
	{
		dbg("name %s is NOT array in json!!!", name);
		return default_value;
	}
	return request[name];
}

static mstester_config * parse_mstester_config(mstester_t obj, const char * config_json){
	int ret = -1;
	mstester_config * cfg = new mstester_config;
	Json_em::Reader reader;
	Json_em::Value value;

	do {
		if(!config_json){
			config_json = "{}";
		}

		if (!reader.parse(config_json, value)) {
			dbge("parse config JSON fail!!!");
			ret = -1;
			break;
		}

		cfg->peer_host = _json_get_string(value, "peerHost", std::string("127.0.0.1"));
		cfg->peer_port = _json_get_int(value, "peerPort", 5000);
        cfg->pkt_size = _json_get_int(value, "packetSize", 62);
        cfg->bitrate = _json_get_int(value, "bitrate", 8000);
		cfg->test_seconds = _json_get_int(value, "testSeconds", 10);

        


        ret = 0;
	} while (0);

	if(ret != 0){
		delete cfg;
		cfg = 0;
	}
	return cfg;
}

static void dump_config(mstester_t obj, mstester_config * cfg){
    dbgi("=== mstester_config ===");
    
    pj_log_push_indent();
	dbgi("peer %s:%d", cfg->peer_host.c_str(), cfg->peer_port);
    dbgi("bitrate: %d bps", cfg->bitrate);
    dbgi("pkt_size: %d ", cfg->pkt_size);
    dbgi("test_seconds: %d", cfg->test_seconds);
    pj_log_pop_indent();
}


static pj_uint32_t build_serv_echo_packet(mstester_t obj, pj_uint32_t seq, pj_uint8_t buf[] )
{
    
    pj_uint32_t offset = 0;
    buf[offset] = PKT_TYPE_CTRL; // command
    offset += 1;
    
    buf[offset] = CTRL_EVENT_TEST_ECHO; // msg type
    offset += 1;
    
    be_set_u32(obj->sessionId, &buf[offset]); // session id
    offset += 4;
    
    be_set_u32(obj->pkt_seq, &buf[offset]); // seq
    offset += 4;
    
    
    pj_timestamp t;
    pj_get_timestamp(&t);
    be_set_u32(t.u32.lo, &buf[offset]); // timestamp low 32
    offset += 4;
    be_set_u32(t.u32.hi, &buf[offset]); // timestamp high 32
    offset += 4;
    
    
    return offset;
}


static void _recv_echo_packet(mstester_t obj, pj_uint8_t buf[], pj_uint32_t len )
{
    int ret = -1;
    do{
        if(len < 8){
            dbge("packet too short");
            break;
        }
        
        pj_uint32_t offset = 0;
        if(buf[offset] != PKT_TYPE_CTRL){
            dbge("packet command error");
            break;
        }
        offset += 1;
        
        if(buf[offset] != CTRL_EVENT_TEST_ECHO){
            dbge("packet msg error");
            break;
        }
        offset += 1;
        
        
        pj_uint32_t sid = be_get_u32(&buf[offset]);
        offset += 4;
        if(sid != obj->sessionId){
            dbge("packet session_id error");
            break;
        }
        
        pj_uint32_t seq = be_get_u32(&buf[offset]);
        offset += 4;
        if(seq == 0){
            dbgi("ignore warming up packet");
            ret = 0;
            break;
        }
        
        
        obj->recv_pkts++;
        obj->recv_bytes += len;
        
        
        pj_uint32_t expect = (obj->expect_recv_seq);
        obj->expect_recv_seq = seq +1;
        
        if(seq > expect)
        {
            pj_uint32_t lost = seq - expect;
            dbgi("seq: expect=%u, recv=%u, lost=%u",  expect, seq, lost );
            obj->seq_lost_count += lost;
            break;
        }
        
        if(seq < expect)
        {
            dbgi("disorder seq: expect=%u, recv=%u",  expect, seq );
            obj->disorder_count++;
            break;
        }
        
        
        
        pj_uint32_t lo = be_get_u32(&buf[offset]); // timestamp low 32
        offset += 4;
        pj_uint32_t hi = be_get_u32(&buf[offset]); // timestamp high 32
        offset += 4;
        pj_timestamp send_time;
        pj_set_timestamp32(&send_time, hi, lo);
        pj_timestamp recv_time;
        pj_get_timestamp(&recv_time);
        
        pj_uint32_t delay = pj_elapsed_msec(&send_time, &recv_time);
        statics_item_update(&obj->st_delay, delay);
        
        
        ret = 0;
        
    }while(0);
    
    
    if(ret != 0){
        obj->err_pkts++;
    }
}




static int _build_echo_pkt_and_send(mstester_t obj)
{
    obj->pkt_len = build_serv_echo_packet(obj, obj->pkt_seq, obj->pkt);
    obj->pkt_seq++;
    obj->pkt_len = obj->cfg->pkt_size;
    
    pj_ssize_t pktsz = obj->pkt_len;
//    pj_activesock_sendto(obj->sock.active_sock, &obj->sock.send_key, obj->pkt,
//                         &pktsz, 0, &obj->peer_addr,
//                         pj_sockaddr_get_len(&obj->peer_addr));
//    
    int ret = pj_sock_sendto(obj->sock.sock_fd, obj->pkt, &pktsz, 0, &obj->peer_addr, pj_sockaddr_get_len(&obj->peer_addr));
    if(ret != PJ_SUCCESS){
        pj_str_t es = pj_strerror(ret, obj->err_str, sizeof(obj->err_str));
        dbge("sending NO.%d pkt fail , ret=%d (%s)!!!", obj->sent_pkts, ret, es.ptr);
        return ret;
    }
    
    obj->sent_pkts++;
    obj->sent_bytes += pktsz;
    
    obj->sending_pkts++;
    obj->sending_bytes += pktsz;
    return 0;
}



static void _schedule_timer(mstester_t obj)
{
    pj_time_val delay;
    delay.sec = 0;
    delay.msec = 2000;
    pj_timer_heap_schedule_w_grp_lock(obj->timer_heap,
                                      &obj->timer, &delay, PJ_TRUE, obj->grp_lock);
}


static void timer_ice_cb(pj_timer_heap_t *th, pj_timer_entry *te)
{
	mstester_t  obj;
    
    obj = (mstester_t) te->user_data;
    
    PJ_UNUSED_ARG(th);
    
    pj_grp_lock_acquire(obj->grp_lock);
    
    pj_bool_t is_cont = PJ_TRUE;
    
    do{
        pj_timestamp now;
        pj_get_timestamp(&now);
        
        if(obj->state == US_FINISH){
            dbgi("test already stopped, stop timer now");
            is_cont = PJ_FALSE;
            break;
        }
        
        if(obj->state == US_RUNNING) {
            pj_uint32_t elapsed_ms = pj_elapsed_msec(&obj->check_time, &now);
            
            pj_uint64_t bytes = obj->sent_bytes - obj->last_sent_bytes;
            obj->send_bps = (elapsed_ms == 0 ? bytes*8 : bytes*8*1000/elapsed_ms);
            obj->last_sent_bytes = obj->sent_bytes;
            
            bytes = obj->recv_bytes - obj->last_recv_bytes;
            obj->recv_bps = (elapsed_ms == 0 ? bytes*8 : bytes*8*1000/elapsed_ms);
            obj->last_recv_bytes = obj->recv_bytes;
            
            obj->check_time = now;
            dbgi("realtime: bitrate=%llu/%llu, bytes=%llu/%llu, pkts=%u/%u"
                 , obj->send_bps, obj->recv_bps
                 , obj->recv_bytes, obj->sent_bytes
                 , obj->recv_pkts , obj->sent_pkts
                 );
            break;
            
        }
        
    }while(0);
    
    
    if(is_cont){
        _schedule_timer(obj);
    }
    
    
    pj_grp_lock_release(obj->grp_lock);
    
    
}

static pj_bool_t on_data_recvfrom(pj_activesock_t *asock,
                                  void *data,
                                  pj_size_t size,
                                  const pj_sockaddr_t *src_addr,
                                  int addr_len,
                                  pj_status_t status)
{
    mstester_t obj = (mstester_t) pj_activesock_get_user_data(asock);
    pj_grp_lock_acquire(obj->grp_lock);
    
    _recv_echo_packet(obj, (pj_uint8_t *)data, (pj_uint32_t)size);
    
    pj_grp_lock_release(obj->grp_lock);
    
    return PJ_TRUE; // need to recv more data
}


static pj_bool_t on_data_sent(pj_activesock_t *asock,
                              pj_ioqueue_op_key_t *send_key,
                              pj_ssize_t sent)
{
    mstester_t obj = (mstester_t) pj_activesock_get_user_data(asock);
    pj_bool_t is_cont = PJ_FALSE;
    pj_grp_lock_acquire(obj->grp_lock);
    
    do{
        obj->sent_pkts++;
        obj->sent_bytes += sent;
        
        if(obj->state != US_RUNNING) {
            dbgi("NOT running, stop sending");
            break;
        }
        
        int ret = _build_echo_pkt_and_send(obj);
        if(ret != PJ_SUCCESS){
            break;
        }
        
        is_cont = PJ_TRUE;
    }while (0);
    
    pj_grp_lock_release(obj->grp_lock);
    
    return is_cont; // need to send more data
}


static int create_pjsock(mstester_t obj, pjsock * sock)
{
    int ret = -1;
    void * user_data = obj;
    do{
        int af = pj_AF_INET();
        sock->sock_fd = PJ_INVALID_SOCKET;
        pj_ioqueue_op_key_init(&sock->send_key, sizeof(sock->send_key));
        
        
        ret = pj_sock_socket(af, pj_SOCK_DGRAM(), 0, &sock->sock_fd);
        if (ret != PJ_SUCCESS) {
            dbge("create socket fail!!!");
            break;
        }
        
        pj_uint16_t	port_range = 0;
        pj_uint16_t max_bind_retry = 100;
        pj_sockaddr_init(af, &sock->bound_addr, NULL, 0);
        ret = pj_sock_bind_random(sock->sock_fd, &sock->bound_addr,
                                  port_range, max_bind_retry);
        if (ret != PJ_SUCCESS){
            dbge("bind socket fail!!!");
            break;
        }
        
        int addr_len = sizeof(sock->bound_addr);
        ret = pj_sock_getsockname(sock->sock_fd, &sock->bound_addr,
                                  &addr_len);
        if (ret != PJ_SUCCESS) {
            dbge("get socket sock name fail!!!");
            break;
        }
        
        unsigned sobuf_size = 200*1000;
        ret = pj_sock_setsockopt_sobuf(sock->sock_fd, pj_SO_RCVBUF(), PJ_TRUE, &sobuf_size);
        if (ret != PJ_SUCCESS) {
            dbge("set socket recv buff size %d fail!!!", sobuf_size);
            break;
        }
        dbge("set socket recv buff size %d ", sobuf_size);
        
        sobuf_size = 200*1000;
        ret = pj_sock_setsockopt_sobuf(sock->sock_fd, pj_SO_SNDBUF(), PJ_TRUE, &sobuf_size);
        if (ret != PJ_SUCCESS) {
            dbge("set socket send buff size %d fail!!!", sobuf_size);
            break;
        }
        dbge("set socket send buff size %d ", sobuf_size);
        
        
        pj_activesock_cfg activesock_cfg;
        pj_activesock_cfg_default (&activesock_cfg);
        activesock_cfg.grp_lock = obj->grp_lock;
        activesock_cfg.async_cnt = 1;
        activesock_cfg.concurrency = 0;
        
        pj_activesock_cb activesock_cb;
        pj_bzero(&activesock_cb, sizeof(activesock_cb));
        activesock_cb.on_data_recvfrom = &on_data_recvfrom;
        activesock_cb.on_data_sent = &on_data_sent;
        
        ret = pj_activesock_create(obj->ice_pool, sock->sock_fd,
                                   pj_SOCK_DGRAM(), &activesock_cfg, obj->ioqueue, &activesock_cb,
                                   user_data, &sock->active_sock);
        if (ret != PJ_SUCCESS){
            dbge("create active sock fail!!!");
            break;
        }
        
        ret = pj_activesock_start_recvfrom(sock->active_sock, obj->ice_pool, 2*1024, 0);
        if (ret != PJ_SUCCESS) {
            dbge("start recv from fail!!!");
            break;
        }
        
        ret = 0;
    }while(0);
    return ret;
}

static void free_pjsock(mstester_t obj, pjsock * sock)
{
    if (sock->active_sock != NULL) {
        sock->sock_fd = PJ_INVALID_SOCKET;
        pj_activesock_close(sock->active_sock);
    } else if (sock->sock_fd && sock->sock_fd != PJ_INVALID_SOCKET) {
        pj_sock_close(sock->sock_fd);
        sock->sock_fd = PJ_INVALID_SOCKET;
    }
}

static int build_pj_sockaddr(mstester_t obj, const char * ip, int port, pj_sockaddr * saddr )
{
    int af = pj_AF_INET();

    pj_str_t strIp = pj_str((char *) ip);
    
    saddr->addr.sa_family = (pj_uint16_t) af;
    
//    const pj_sockaddr *sa = (const pj_sockaddr*)saddr;
    
    void * a = pj_sockaddr_get_addr(saddr);
    int ret = pj_inet_pton(af, &strIp, a);
    if (ret != PJ_SUCCESS) {
        return ret;
    }
    pj_sockaddr_set_port(saddr, port);
    return 0;
    
}



int mstester_new(const char* config, mstester_t * pobj){

	int ret = -1;
	mstester_t obj = 0;
	mstester_config * cfg = 0;

    
	_auto_register_pj_thread();
    
	dbg("mstester_new");
    

	do {
		obj = (mstester_t) malloc(sizeof(struct mstester_st));
        memset(obj, 0, sizeof(struct mstester_st));

        pj_ansi_snprintf(obj->obj_name, sizeof(obj->obj_name),
                         "mstest%p", obj);
		
        
		// create pool factory
		pj_caching_pool_init(&obj->cp, NULL, 0);
		obj->cp_inited = 1;


		/* Create application memory pool */
		obj->ice_pool = pj_pool_create(&obj->cp.factory, "mstest_pool", 512, 512, NULL);
        
        dbgi("config: %s", config);
        
        cfg = parse_mstester_config(obj, config);
        if(!cfg){
            ret = -1;
            break;
        }
        dump_config(obj, cfg);
        obj->cfg = cfg;

        
        ret = pj_grp_lock_create(obj->ice_pool, NULL, &obj->grp_lock);
        if (ret != PJ_SUCCESS) {
            dbge("failed to create grp lock, ret=%d", ret);
            break;
        }
        
        pj_grp_lock_add_ref(obj->grp_lock);
        
        obj->timer.cb = &timer_ice_cb;
        obj->timer.user_data = obj;


		/* Create timer heap for timer stuff */
		ret = pj_timer_heap_create(obj->ice_pool, 100,
				&obj->timer_heap);
		if (ret != PJ_SUCCESS) {
			dbge("failed to create timer heap, ret=%d", ret);
			break;
		}

		/* and create ioqueue for network I/O stuff */
		ret = pj_ioqueue_create(obj->ice_pool, 16, &obj->ioqueue);
		if (ret != PJ_SUCCESS) {
			dbge("failed to create ioqueue, ret=%d", ret);
			break;
		}

		/* something must poll the timer heap and ioqueue,
		 * unless we're on Symbian where the timer heap and ioqueue run
		 * on themselves.
		 */
		ret = pj_thread_create(obj->ice_pool, "eice_thread",
				&_worker_thread_for_pj, obj, 0, 0, &obj->ice_thread);
		if (ret != PJ_SUCCESS) {
			dbge("failed to create worker thread, ret=%d", ret);
			break;
		}

        
        ret = create_pjsock(obj, &obj->sock);
        if (ret != PJ_SUCCESS) {
            pj_str_t es = pj_strerror(ret, obj->err_str, sizeof(obj->err_str));
            dbge("failed to create worker sock, ret=%d (%s)", ret, es.ptr);
            break;
        }


		*pobj = obj;
		ret = 0;

	} while (0);

	if(ret != 0){
		mstester_free(obj);
	}

	return ret;
}

int mstester_get_status(mstester_t obj, char * content, int * p_content_len){
	int ret = 0;
	_auto_register_pj_thread();
	dbgi("mstester_get_status");

    Json_em::Value root_val;
	pj_grp_lock_acquire(obj->grp_lock);
    root_val["state"] = obj->state;
    root_val["sentBytes"] = obj->sent_bytes;
    root_val["sentPackets"] = obj->sent_pkts;
    root_val["recvBytes"] = obj->recv_bytes;
    root_val["recvPackets"] = obj->recv_pkts;
    root_val["lostPackets"] = obj->sent_pkts-obj->recv_pkts;
    root_val["lostSeqs"] = obj->seq_lost_count;
    root_val["disorders"] = obj->disorder_count;
    
    root_val["delayAverage"] = obj->st_delay.aver;
    root_val["delayMin"] = obj->st_delay.min;
    root_val["delayMax"] = obj->st_delay.max;
    root_val["delayLast"] = obj->st_delay.last;
    
    root_val["sendBitrate"] = obj->send_bps;
    root_val["recvBitrate"] = obj->recv_bps;

	pj_grp_lock_release(obj->grp_lock);
    
    Json_em::FastWriter writer;
    std::string str = writer.write(root_val);
    strcpy(content, str.c_str());
    *p_content_len = (int) str.size();

    if(*p_content_len > 0){
        dbgi("status = %s", content);
    }
    
	return ret;
}

int mstester_get_state(mstester_t obj)
{
    pj_grp_lock_acquire(obj->grp_lock);
    int s = obj->state;
    pj_grp_lock_release(obj->grp_lock);
    return s;
}

void mstester_free(mstester_t obj){
	if(!obj) return ;

	_auto_register_pj_thread();

    dbgi("mstester_free");
    
    free_pjsock(obj, &obj->sock);
    
    if(obj->grp_lock){
        pj_timer_heap_cancel_if_active(obj->timer_heap, &obj->timer, 0);
    }
    
    
	if(obj->status_content){
		delete obj->status_content;
		obj->status_content = 0;
	}


    
    // stop sending thread
//    obj->send_thread_stop_req = PJ_TRUE;
//    if(obj->send_thread){
//        pj_thread_join(obj->send_thread);
//        pj_thread_destroy(obj->send_thread);
//        obj->send_thread = 0;
//    }
    mstester_stop(obj);
    
    
    // stop thread
    obj->thread_quit_flag = PJ_TRUE;
    if (obj->ice_thread) {
        pj_thread_join(obj->ice_thread);
        pj_thread_destroy(obj->ice_thread);
        obj->ice_thread = 0;
    }


    // stop io queue
    if (obj->ioqueue) {
        pj_ioqueue_destroy(obj->ioqueue);
        obj->ioqueue = 0;
    }


    // stop timer
    if (obj->timer_heap) {
        pj_timer_heap_destroy(obj->timer_heap);
        obj->timer_heap = 0;
    }

    
    if(obj->grp_lock){
        pj_status_t s;
        //        dbgi("destroying lock 0x%p", obj->grp_lock);
        pj_grp_lock_acquire(obj->grp_lock);
        s = pj_grp_lock_dec_ref(obj->grp_lock);
        //        dbgi("dec ref lock return %d", s);
        
        s = pj_grp_lock_release(obj->grp_lock);
        //        dbgi("release lock return %d", s);
        
        if(s != PJ_EGONE){
            dbge("mstester: release lock return %d, NOT PJ_EGONE !!!", s);
        }
        obj->grp_lock = 0;
    }
    
    
    

    if(obj->cfg){
    	delete obj->cfg;
    	obj->cfg = 0;
    }

    // stop caching pool
    if(obj->cp_inited){
    	pj_caching_pool_destroy(&obj->cp);
    	obj->cp_inited = 0;
    }

	free(obj);
}


static int _sending_thread(void *arg) {
    
    mstester_t obj = (mstester_t) arg;
    
    _auto_register_pj_thread();
    
    
    int ret = 0;
    
    
    
    pj_grp_lock_acquire(obj->grp_lock);
    
    pj_get_timestamp(&obj->kick_time);
    obj->sessionId = obj->kick_time.u32.lo%1000000;
    
    
    // send warming up packet
    obj->pkt_len = build_serv_echo_packet(obj, 0, obj->pkt);
    obj->pkt_len = obj->cfg->pkt_size;
    
    
    unsigned warm_up_pkts = 8;
    dbgi("sending warming up, %d packets ...\n", warm_up_pkts);
    for(unsigned i = 0; i < warm_up_pkts; i++){
        pj_ssize_t pktsz = obj->pkt_len;
        ret = pj_sock_sendto(obj->sock.sock_fd, obj->pkt, &pktsz, 0, &obj->peer_addr, pj_sockaddr_get_len(&obj->peer_addr));
    }
    dbgi("sending warming up, %d packets done\n", warm_up_pkts);
    
    
    
    obj->pkt_seq = 1;
    obj->expect_recv_seq = 1;
    
    
    dbgi("sending packet (%u bytes) at bitrate %u kbps in %u seconds ...",
         obj->cfg->pkt_size, obj->cfg->bitrate/1000, obj->cfg->test_seconds);
    
    
    pj_get_timestamp(&obj->kick_time);
    pj_get_timestamp(&obj->check_time);
    
    
    
    while (!obj->send_thread_stop_req) {
        
        ret = _build_echo_pkt_and_send(obj);
        if(ret != PJ_SUCCESS){
            break;
        }
        
        
        pj_timestamp now;
        pj_get_timestamp(&now);
        pj_uint64_t elapsed_ms = pj_elapsed_msec64(&obj->kick_time, &now);
        pj_uint64_t normal_ms = obj->sent_bytes * 1000 * 8 / obj->cfg->bitrate;
        
        if(elapsed_ms >= obj->cfg->test_seconds * 1000){
            dbgi("reach test seconds %u", obj->cfg->test_seconds);
            break;
        }
        
        pj_grp_lock_release(obj->grp_lock);
        
        if(normal_ms > elapsed_ms)
        {
            pj_uint32_t ms = (pj_uint32_t)(normal_ms - elapsed_ms);
//            dbgi("delay %u ms, sent packets %u\n", ms , obj->sent_pkts );
            pj_thread_sleep(ms);
        }
        
        pj_grp_lock_acquire(obj->grp_lock);
    }
    
    pj_timestamp now;
    pj_get_timestamp(&now);
    pj_uint64_t sent_elapsed_ms = pj_elapsed_msec64(&obj->kick_time, &now);
    
    
    pj_grp_lock_release(obj->grp_lock);
    
    // waiting for last packet recved
    dbgi("waiting for last packet...");
    pj_thread_sleep(2*1000);
    
    
    pj_grp_lock_acquire(obj->grp_lock);
    
    obj->send_bps = (sent_elapsed_ms == 0 ? obj->sent_bytes*8 : obj->sent_bytes*8*1000/sent_elapsed_ms);
    obj->recv_bps = (sent_elapsed_ms == 0 ? obj->recv_bytes*8 : obj->recv_bytes*8*1000/sent_elapsed_ms);
    dbgi("sent %u bytes, elapse %u ms, bitrate %u bps", obj->sent_bytes, sent_elapsed_ms, obj->send_bps);
    
    obj->state = US_FINISH;
    
    pj_grp_lock_release(obj->grp_lock);
    
    
    dbgi("sending thread exited");
    
    return 0;
}


int mstester_start(mstester_t obj)
{
    int ret = -1;
    
    _auto_register_pj_thread();
    
    pj_grp_lock_acquire(obj->grp_lock);
    
    do{
        if(obj->state == US_RUNNING) {
            dbgw("already running");
            ret = 0;
            break;
        }
        
        if(obj->state != US_READY && obj->state != US_FINISH) {
            dbgw("state expect US_READY US_FINISH, but %d", obj->state);
            ret = -1;
            break;
        }
        
        statics_item_init(&obj->st_delay);
        
        
        ret = build_pj_sockaddr(obj, obj->cfg->peer_host.c_str(), obj->cfg->peer_port, &obj->peer_addr );
        if(ret != PJ_SUCCESS){
            dbge("build peer address fail !!!");
            break;
        }
        
        obj->send_thread_stop_req = PJ_FALSE;
        ret = pj_thread_create(obj->ice_pool, "sending_thread",
                               &_sending_thread, obj, 0, 0, &obj->send_thread);
        if (ret != PJ_SUCCESS) {
            dbge("failed to create sending thread, ret=%d", ret);
            break;
        }
        
        obj->state = US_RUNNING;
        
        _schedule_timer(obj);
        
        ret = 0;
        
    }while(0);
    
    pj_grp_lock_release(obj->grp_lock);
    
    return ret;
}

void mstester_stop(mstester_t obj)
{
    int ret = -1;
    
    pj_grp_lock_acquire(obj->grp_lock);
    
    do{
        if(obj->state != US_RUNNING) {
            dbgw("NOT running when try stopping");
            ret = 0;
            break;
        }
    
        // stop sending thread
        obj->send_thread_stop_req = PJ_TRUE;
        
        if(obj->send_thread){
            pj_grp_lock_release(obj->grp_lock);
            pj_thread_join(obj->send_thread);
            pj_thread_destroy(obj->send_thread);
            obj->send_thread = 0;
            pj_grp_lock_acquire(obj->grp_lock);
        }
        
        
        ret = 0;
        
    }while(0);
    
    pj_grp_lock_release(obj->grp_lock);
    
}

void mstester_waitfor_done(mstester_t obj)
{
    _auto_register_pj_thread();
    pj_grp_lock_acquire(obj->grp_lock);
    
    while (obj->state == US_RUNNING) {
        pj_grp_lock_release(obj->grp_lock);
        pj_thread_sleep(500);
        pj_grp_lock_acquire(obj->grp_lock);
    }
    
    pj_grp_lock_release(obj->grp_lock);
    
}



static pj_status_t handle_events(mstester_t obj, unsigned max_msec, unsigned *p_count) {

//    register_eice_thread(g_unit->pool);

    enum {
        MAX_NET_EVENTS = 1
    };
    pj_time_val max_timeout = {0, 0};
    pj_time_val timeout = { 0, 0};
    unsigned count = 0, net_event_count = 0;
    int c;

    max_timeout.msec = max_msec;

    /* Poll the timer to run it and also to retrieve the earliest entry. */
    timeout.sec = timeout.msec = 0;
    c = pj_timer_heap_poll( obj->timer_heap, &timeout );
    if (c > 0) {
        count += c;
    }

    /* timer_heap_poll should never ever returns negative value, or otherwise
     * ioqueue_poll() will block forever!
     */
    pj_assert(timeout.sec >= 0 && timeout.msec >= 0);
    if (timeout.msec >= 1000) {
        timeout.msec = 999;
    }

    /* compare the value with the timeout to wait from timer, and use the
     * minimum value.
     */
    if (PJ_TIME_VAL_GT(timeout, max_timeout)) {
        timeout = max_timeout;
    }

    /* Poll ioqueue.
     * Repeat polling the ioqueue while we have immediate events, because
     * timer heap may process more than one events, so if we only process
     * one network events at a time (such as when IOCP backend is used),
     * the ioqueue may have trouble keeping up with the request rate.
     *
     * For example, for each send() request, one network event will be
     *   reported by ioqueue for the send() completion. If we don't poll
     *   the ioqueue often enough, the send() completion will not be
     *   reported in timely manner.
     */
    do {
        c = pj_ioqueue_poll( obj->ioqueue, &timeout);
        if (c < 0) {
            pj_status_t err = pj_get_netos_error();
            pj_thread_sleep((unsigned int)PJ_TIME_VAL_MSEC(timeout));
            if (p_count)
                *p_count = count;
            return err;
        } else if (c == 0) {
            break;
        } else {
            net_event_count += c;
            timeout.sec = timeout.msec = 0;
        }
    } while (c > 0 && net_event_count < MAX_NET_EVENTS);

    count += net_event_count;
    if (p_count) {
        *p_count = count;
    }

    return PJ_SUCCESS;

}

static int _worker_thread_for_pj(void *arg) {

	mstester_t obj = (mstester_t) arg;

//    register_eice_thread(g_unit->pool);

    while (!obj->thread_quit_flag) {
        handle_events(obj, 500, NULL);
    }
    
    dbgi("work thread exited");

    return 0;
}





int test_mstester(){
    
    const char * config_json = "{"
    
//    "\"peerHost\":\"172.16.3.234\""
    "\"peerHost\":\"121.41.87.159\""

    ",\"peerPort\":5000"
    ",\"testSeconds\":10"
    
    ",\"packetSize\":1024"
    ",\"bitrate\":600000"
    
    
    "}";
    
    pj_log_set_level(3);
    
    mstester_t obj = 0;
    int ret = -1;
    
    do{
        ret = mstester_new(config_json, &obj);
        if(ret != PJ_SUCCESS){
            dbge("mstester_new fail !!!");
            break;
        }
        
        ret = mstester_start(obj);
        if(ret != PJ_SUCCESS){
            dbge("mstester_new fail !!!");
            break;
        }
        
//        mstester_waitfor_done(obj);
      
        char status_content[1024];
        int status_content_len = sizeof(status_content);
        while (mstester_get_state(obj) != US_FINISH) {
            
            status_content_len = sizeof(status_content);
            mstester_get_status(obj, status_content, &status_content_len);
            
            pj_thread_sleep(1000);
        }
        
        status_content_len = sizeof(status_content);
        mstester_get_status(obj, status_content, &status_content_len);
        if(status_content_len > 0){
            dbgi("final status =%s", status_content);
        }
        
        
    }while(0);
    
    if(obj){
        mstester_free(obj);
        obj = 0;
    }
    
    return ret;
}










