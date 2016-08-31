
#include <stdio.h>
#include <string>
#include <pjlib.h>
#include <pjlib-util.h>
#include <pjnath.h>
#include <string.h>
#include <json/json.h>
#include <eice.h>

// for ipv6
# include <arpa/inet.h>
# include <sys/types.h>
# include <sys/socket.h>
# include <sys/select.h>
# include <netinet/in.h>
# include <unistd.h>
# include <string.h>
# include <errno.h>
# include <netdb.h>
# include <fcntl.h>


//#define DEBUG_RELAY

#define dbg(...) PJ_LOG(3, (__FILE__, __VA_ARGS__))
#define dbgi(...) PJ_LOG(3, (obj->obj_name, __VA_ARGS__))
#define dbgw(...) PJ_LOG(2, (obj->obj_name, __VA_ARGS__))
#define dbge(...) PJ_LOG(1, (obj->obj_name, __VA_ARGS__))

#define MAX_IP_SIZE 64

typedef struct tag_confice * confice_t;


//#define CONFICE_ST_READY 0
//#define CONFICE_ST_REGING 1
//#define CONFICE_ST_REG_DONE 2
//#define CONFICE_ST_NEGOING 3
//#define CONFICE_ST_NEGO_DONE 4
//#define CONFICE_ST_NOMING 5
//#define CONFICE_ST_NOM_DONE 6
//#define CONFICE_ST_FINAL 99
typedef enum confice_op
{
    CONF_OP_NONE = 0,
    
    CONF_OP_REGING,
    CONF_OP_REG_DONE,
    CONF_OP_NEGOING,
    CONF_OP_NEGO_DONE,
    CONF_OP_SELECTING,
    CONF_OP_SELECT_DONE,
    CONF_OP_FINAL = 99,
    
    CONF_OP_MAX
    
} confice_op;

typedef struct confice_cb
{
    void (*on_complete)(confice_t obj, confice_op op, int status);
} confice_cb;


typedef struct{
	confice_t owner;
    pj_uint32_t comp_id;
	int channelId;
	pj_sockaddr bound_addr;
    
    pj_sockaddr bound_addr_fake;  // fake for ice
    pj_sockaddr remote_addr_fake;
    
	pj_sock_t sock_fd; /* Socket descriptor	    */
	pj_activesock_t *active_sock; /* Active socket object	    */
	pj_ioqueue_op_key_t int_send_key; /* Default send key for app */

    int is_reg;
    
    pj_uint8_t pkt_ice[2*1024];
    pj_uint32_t pkt_ice_len;
    
    
    pj_uint32_t tsx_id;
    pj_uint8_t pkt_common[1024];
    pj_uint32_t pkt_common_len;
    
    
    unsigned cand_id;
    int is_relay_done;

}confchannel;



enum tp_type
{
    TP_NONE,
    TP_STUN,
    TP_TURN
};

#if PJNATH_ICE_PRIO_STD
#   define SRFLX_PREF  65535
#   define HOST_PREF   65535
#   define RELAY_PREF  65535
#else
#   define SRFLX_PREF  0
#   define HOST_PREF   0
#   define RELAY_PREF  0
#endif


#define CALLER_FAKE_IP "10.10.10.10"
#define CALLEE_FAKE_IP "10.10.10.11"
#define CICE_DEFAULT_TIMEOUT_MS  4000
#define ICE_TIMEOUT_MS 4000

struct tag_confice{
    char		obj_name[PJ_MAX_OBJ_NAME];
    confice_cb  cb;
    void * user_data;
    
	pj_pool_t * pool;
	pj_ioqueue_t * ioqueue;
	pj_grp_lock_t	*grp_lock;
	pj_timer_heap_t	*timer_heap;
    pj_ice_strans_cfg * ice_cfg;
    pj_ice_sess_role role;
    
	std::string * confId;
	std::string * serverIp;
	std::string * rcode;
	int serverPort;
	int channelCount;
	confchannel * channels;
    pj_uint32_t timeout_ms;
    
	pj_timer_entry	 timer;
	pj_sockaddr  server_addr;

	confice_op state;
	int reg_kicked;
	int reg_done;
    pj_uint32_t reg_send_count;
	pj_timestamp reg_send_time;

    
	int sel_relay_kicked;
	int sel_relay_done;
	pj_timestamp sel_relay_send_time;
    
    pj_uint32_t sessionId;
    pj_ice_sess * ice_sess;
    std::string * str_local_ufrag;
    std::string * str_local_pwd;
    std::string * str_remote_ufrag;
    std::string * str_remote_pwd;
    pj_ice_sess_cand remote_cands[2];
    
    int final_result;
    int env_af;

};


#define PKT_TYPE_HEARTBEAT		0xff
#define PKT_TYPE_REGISTER		0xfe
#define PKT_TYPE_CTRL			0xfc
#define PKT_TYPE_MIN			0xfb
#define PKT_TYPE_MSG            0xF7

#define CTRL_EVENT_TOKENT_CHANGED   	1
#define CTRL_EVENT_TOKENT_RELEASE		2

#define MSG_TYPE_ICE_WRAP               0
#define MSG_TYPE_SELECT_RELAY           1
#define MSG_TYPE_SELECT_RELAY_ACK       2
//#define MSG_TYPE_ICE_PING               3
//#define MSG_TYPE_ICE_PING_ACK           4



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

nego_result_func g_nego_result_func = NULL;

static void get_ip_port_from_sockaddr(const pj_sockaddr *addr, char *address, int *port) {

    if(pj_sockaddr_has_addr(addr)){
        *port = (int) pj_sockaddr_get_port(addr);
        pj_sockaddr_print(addr, address, MAX_IP_SIZE, 0);
    }else{
        *port = 0;
        address[0] = '\0';
    }

}


static pj_uint8_t _calc_checksum(const pj_uint8_t data[], pj_uint32_t datalen){
    pj_uint8_t chk = 0;
    for (pj_uint32_t i = 0; i < datalen; i++) {
        chk ^= data[i];
    }
    return chk;
}

static bool _verify_checksum(const pj_uint8_t data[], pj_uint32_t datalen)
{
	pj_uint8_t chk = _calc_checksum(data, datalen-1);
	if(chk==data[datalen-1]) return true;
	return false;
}

// for ipv6

static
int ip_to_addr_family(const char *ip){
    if(ip == NULL)
    {
        return pj_AF_INET();
    }
    int af = strchr(ip, ':') ? pj_AF_INET6() : pj_AF_INET();
    return af;
}

static
int ipv4ToIpv6(const char * ipv4, char ipv6[]){
    struct addrinfo *answer, hint, *curr;
    bzero(&hint, sizeof(hint));
    hint.ai_family = AF_UNSPEC;
    hint.ai_socktype = SOCK_DGRAM;
    int ret = getaddrinfo(ipv4, "6718", &hint, &answer);
    if(ret != 0){
        dbg("getaddrinfo ret %d !!!", ret);
        return -1;
    }
    
    for (curr = answer; curr != NULL; curr = curr->ai_next){
        if (curr->ai_family == AF_INET6) {
            char name[128] = {0};
            char service[16] = {0};
            getnameinfo(curr->ai_addr, curr->ai_addrlen, name, sizeof(name), service, sizeof(service), 0);
            if (!strcmp(name, "0")) {
                continue;
            }
            dbg("ipv6: %s -> name[%s], service[%s]", ipv4, name, service);
            if(ipv6){
                strcpy(ipv6, name);
            }
            return 0;
        }
    }
    
    dbg("NOT found ipv6");
    
    return -1;
}


// make pj_sockaddr according to ip type (ipv4 or ipv6)
static
int make_addr_v2(pj_pool_t *ice_pool, const char * ip, int port, int target_af, pj_sockaddr * addr, char ipv6[]){
    pj_str_t str;
    ipv6[0] = '\0';
    int empty_ip = (!ip) || (ip[0]=='\0');
    int af = ip_to_addr_family(ip);
    if(af != pj_AF_INET6() && target_af == pj_AF_INET6() && !empty_ip){
//        char ipv6[128];
        if(ipv4ToIpv6(ip, ipv6) == 0){
            //                        dbgi("convert ipv6: %s", ipv6);
            pj_strdup2_with_null(ice_pool, &str, ipv6);
            af = pj_AF_INET6();
        }else{
            dbg("fail convert ipv6 from ip: %s", ip);
            pj_strdup2_with_null(ice_pool, &str, ip);
            af = pj_AF_INET();
        }
        
    }else{
        pj_strdup2_with_null(ice_pool, &str, ip);
    }
    pj_sockaddr_init(af, addr, &str, port);
    return 0;
}

static
int make_addr(pj_pool_t *ice_pool, const char * ip, int port, int target_af, pj_sockaddr * addr){
    char ipv6[128];
    return make_addr_v2(ice_pool, ip, port, target_af, addr, ipv6);
}

//static unsigned _make_reg_pkt_v0(confice_t obj, int channel_id, pj_uint8_t buffer[]) {
//    pj_uint32_t seq = obj->sessionId;
//	buffer[0] = (pj_uint8_t) PKT_TYPE_REGISTER; // command
//	be_set_u32(seq, &buffer[1]); // seq
//
//
//	// conferenceId
//	pj_uint8_t conf_id_len = (pj_uint8_t) (obj->confId->length() &0x0FF);
//	buffer[5] = conf_id_len;// confereneceId length
//	pj_memcpy(&buffer[6], obj->confId->c_str(), conf_id_len);
//	int offset = 6+conf_id_len;
//
//	be_set_u16(channel_id, &buffer[offset]);// channelId
//	offset += 2;
//
//	// checksum
//	buffer[offset] = _calc_checksum(buffer, offset);
//	offset++;
//
//	return offset;
//}

static unsigned _make_reg_pkt_v1(confice_t obj, int channel_id, pj_uint8_t buffer[]) {
    pj_uint32_t seq = obj->sessionId;
    buffer[0] = (pj_uint8_t) PKT_TYPE_REGISTER; // command
    be_set_u32(seq, &buffer[1]); // seq
    
    
    // conferenceId
    pj_uint8_t conf_id_len = (pj_uint8_t) (obj->confId->length() &0x0FF);
    buffer[5] = conf_id_len;// confereneceId length
    if(conf_id_len > 0){
        pj_memcpy(&buffer[6], obj->confId->c_str(), conf_id_len);
    }
    int offset = 6+conf_id_len;
    
    be_set_u32(channel_id, &buffer[offset]);// channelId
    offset += 4;
    
    
    pj_uint8_t rcode_len = (pj_uint8_t)(obj->rcode->length()&0x0FF);
    buffer[offset] = rcode_len;
    offset += 1;
    
    if(rcode_len > 0){
        pj_memcpy(&buffer[offset], obj->rcode->c_str(), rcode_len);
        offset += rcode_len;
    }
    
    
    // checksum
    buffer[offset] = _calc_checksum(buffer, offset);
    offset++;
    
    return offset;
}

unsigned _make_reg_pkt(confice_t obj, int channel_id, pj_uint8_t buffer[]) {
    return _make_reg_pkt_v1(obj, channel_id, buffer);
}

//static bool _check_reg_ack_for_design(confice_t obj, int channel_id, const pj_uint8_t buf[], pj_uint32_t len){
//    if(len < 8){
//        dbgi("reg ack len short than min");
//        return false;
//    }
//    
//    if (!_verify_checksum(buf, len)){
//        dbgi("reg ack check sum fail!!!\n");
//        return false;
//    }
//    
//    pj_uint8_t conf_id_len = buf[5];
//    if((6 + conf_id_len + 2) > len){
//        dbgi("reg ack conf id length too long");
//        return false;
//    }
//    
//    
//    std::string * conf_id = new std::string((const char *)&buf[6], conf_id_len);
//    if((*conf_id) != (*obj->confId)){
//        dbgi("reg ack conf id expect [%s], but [%s]", obj->confId->c_str(), conf_id->c_str());
//        delete conf_id;
//        return false;
//    }
//    
//    pj_uint16_t ch_id = be_get_u16(&buf[6 + conf_id_len]);
//    if(ch_id != channel_id){
//        dbgi("reg ack channel id expect [%u], but [%u]", channel_id, ch_id);
//        delete conf_id;
//        return false;
//    }
//    delete conf_id;
//    return true;
//}

static bool _check_reg_ack(confice_t obj, int channel_id, const pj_uint8_t buf[], pj_uint32_t len){
    if(len < 8){
        dbgi("reg ack len short than min");
        return false;
    }
    
    if (!_verify_checksum(buf, len)){
        dbgi("reg ack check sum fail!!!\n");
        return false;
    }
    
    pj_uint32_t seq = be_get_u32(&buf[1]);
    if(seq != obj->sessionId){
        dbgi("reg ack seq expected %u, but %u\n", obj->sessionId, seq);
        return false;
    }
    
    pj_uint8_t status = buf[5];
    if(status != 0){
        dbgi("reg ack error status %u", status);
        return false;
    }
    
    return true;
}


static void _send_reg(confice_t obj){
    
    dbgi("send reg packet");
	for (int i = 0; i < obj->channelCount; i++) {
		confchannel * ch = &obj->channels[i];
		ch->pkt_common_len = _make_reg_pkt(obj, ch->channelId, ch->pkt_common);
		pj_ssize_t pktsz = ch->pkt_common_len;
		pj_activesock_sendto(ch->active_sock, &ch->int_send_key, ch->pkt_common,
				&pktsz, 0, &obj->server_addr,
				pj_sockaddr_get_len(&obj->server_addr));
	}
    
    if(obj->reg_send_count == 0){
        pj_assert(obj->state < CONF_OP_REGING);
        pj_get_timestamp (&obj->reg_send_time);
        obj->state = CONF_OP_REGING;
        dbgi("kicked reg");
    }
    obj->reg_send_count++;
    
}

static pj_uint32_t _generate_u32(confice_t obj){
    pj_timestamp t;
    pj_get_timestamp(&t);
    pj_uint32_t val = t.u32.lo%1000000;
    return val;
}

static pj_uint32_t _build_msg_header(confice_t obj, confchannel * ch
                                     , unsigned msg_type
                                     , pj_uint32_t tsx_id
                                     , pj_uint32_t data_len
                                     , pj_uint8_t * buf){
    pj_uint32_t offset = 0;
    buf[offset] = PKT_TYPE_MSG; // command
    offset += 1;
    
    buf[offset] = msg_type; // msg type
    offset += 1;
    
    be_set_u32(obj->sessionId, &buf[offset]); // session id
    offset += 4;
    
    be_set_u32(ch->comp_id, &buf[offset]); // comp id
    offset += 4;
    
    be_set_u32(tsx_id, &buf[offset]); // transaction id
    offset += 4;
    
    be_set_u32(data_len, &buf[offset]); // data length
    offset += 4;
    return offset;
}



static void _set_state(confice_t obj, confice_op new_state, int result){
    confice_op old_state= obj->state;
    obj->state = new_state;
    if(old_state < new_state){
        obj->cb.on_complete(obj, new_state, result);
    }
}

static void _set_final(confice_t obj, int result){
    if(obj->state < CONF_OP_FINAL){
        obj->final_result = result;
    }
    _set_state(obj, CONF_OP_FINAL, result);
}


static bool _check_for_select_relay(confice_t obj){
    
    if(obj->state != CONF_OP_NEGO_DONE){
        dbgi("NOT expect state (nego done) for select relay");
        return false;
    }
    
    if(!obj->sel_relay_kicked){
        dbgi("NOT kicked for select relay");
        return false;
    }
    
    if (obj->role != PJ_ICE_SESS_ROLE_CONTROLLING) {
        dbgi("NOT expect controlling role");
        return false;
    }
    
    
    for(unsigned ui = 0; ui < obj->channelCount; ui++){
        confchannel * ch = &obj->channels[ui];
        
        ch->tsx_id = _generate_u32(obj);
        ch->pkt_common_len = _build_msg_header(obj, ch, MSG_TYPE_SELECT_RELAY, ch->tsx_id, (pj_uint32_t)0, ch->pkt_common);
        
        // checksum
        ch->pkt_common[ch->pkt_common_len] = _calc_checksum(ch->pkt_common, ch->pkt_common_len);
        ch->pkt_common_len++;
        
        pj_ssize_t pktsz = ch->pkt_common_len;
        pj_activesock_sendto(ch->active_sock, &ch->int_send_key, ch->pkt_common,
                             &pktsz, 0, &obj->server_addr,
                             pj_sockaddr_get_len(&obj->server_addr));
        
    }
    
//    pj_assert(obj->state < CONF_OP_SELECTING);
    pj_get_timestamp (&obj->sel_relay_send_time);
    obj->state = CONF_OP_SELECTING;
    dbgi("kicked select relay");
    
    return true;
}

static bool _check_for_select_relay_done(confice_t obj){
    if(!obj->sel_relay_done){
        int done = 1;
        for(int i = 0; i < obj->channelCount; i++)
        {
            if(!obj->channels[i].is_relay_done){
                done = 0;
                break;
            }
        }
        
        obj->sel_relay_done = done;
        if(obj->sel_relay_done){
            dbgi("all channel select relay done, change to final state");
            _set_final(obj, 0);
            return true;
        }
    }
    return false;
}



static void on_ccc_ice_complete(pj_ice_sess *ice, pj_status_t status){
    confice_t  obj = (confice_t)ice->user_data;
    dbgi("conf-ice: complete with %d", status);
    
    pj_grp_lock_acquire(obj->grp_lock);
    if(obj->state == CONF_OP_NEGOING){
        _set_state(obj, CONF_OP_NEGO_DONE, status);
        if(status != PJ_SUCCESS){
            dbgi("conf-ice: nego fail");
            _set_final(obj, status);
        }else{
            bool b = _check_for_select_relay(obj);
            dbgi("conf-ice: on-ice-complete's checking relay %s", b ? "true" : "false");
        }
    }
    pj_grp_lock_release(obj->grp_lock);
}

static void on_ccc_ice_rx_data(pj_ice_sess *ice,
                        unsigned comp_id,
                        unsigned transport_id,
                        void *pkt, pj_size_t size,
                        const pj_sockaddr_t *src_addr,
                        unsigned src_addr_len)
{
   // confice_t  obj = (confice_t)ice->user_data;
    
    PJ_UNUSED_ARG(transport_id);
 
}



static pj_status_t on_ccc_ice_tx_pkt(pj_ice_sess *ice,
                              unsigned comp_id,
                              unsigned transport_id,
                              const void *pkt, pj_size_t size,
                              const pj_sockaddr_t *dst_addr,
                              unsigned dst_addr_len)
{
    confice_t  obj = (confice_t)ice->user_data;
    PJ_ASSERT_RETURN(comp_id && comp_id <= obj->channelCount, PJ_EINVAL);
    confchannel * ch = &obj->channels[comp_id-1];
    pj_uint8_t * buf = ch->pkt_ice;
    
    pj_grp_lock_acquire(obj->grp_lock);
    
    pj_uint32_t offset = _build_msg_header(obj, ch, MSG_TYPE_ICE_WRAP, 0, (pj_uint32_t)size, buf);
//    buf[offset] = PKT_TYPE_MSG; // command
//    offset += 1;
//    
//    buf[offset] = MSG_TYPE_ICE_WRAP; // msg type
//    offset += 1;
//    
//    be_set_u32(obj->sessionId, &buf[offset]); // session id
//    offset += 4;
//    
//    be_set_u32(ch->comp_id, &buf[offset]); // comp id
//    offset += 4;
//    
//    be_set_u32(0, &buf[offset]); // transaction id
//    offset += 4;
//    
//    be_set_u32(size, &buf[offset]); // data length
//    offset += 4;
    
    pj_memcpy(&buf[offset], pkt, size);
    offset += size;
    
    
    buf[offset] = _calc_checksum(buf, offset);
    offset++;
    
    pj_ssize_t pktsz = offset;
    pj_status_t status = pj_activesock_sendto(ch->active_sock, &ch->int_send_key, buf,
                         &pktsz, 0, &obj->server_addr,
                         pj_sockaddr_get_len(&obj->server_addr));

    dbgi("ccc-ice session %u tx bytes %u", obj->sessionId, ((pj_uint32_t)pktsz));
    pj_grp_lock_release(obj->grp_lock);
    
    return (status==PJ_SUCCESS||status==PJ_EPENDING) ? PJ_SUCCESS : status;
}



#define CONFICE_CAND_TYPE  PJ_ICE_CAND_TYPE_HOST

static int _start_nego(confice_t obj){
    pj_ice_sess_cb ice_cb;
    pj_bzero(&ice_cb, sizeof(ice_cb));
    ice_cb.on_ice_complete = &on_ccc_ice_complete;
    ice_cb.on_rx_data = &on_ccc_ice_rx_data;
    ice_cb.on_tx_pkt = &on_ccc_ice_tx_pkt;
    
    pj_str_t local_ufrag;
    pj_str_t local_pwd;
    pj_strdup2_with_null(obj->pool, &local_ufrag, obj->str_local_ufrag->c_str());
    pj_strdup2_with_null(obj->pool, &local_pwd, obj->str_local_pwd->c_str());
    
    pj_status_t pjst = PJ_SUCCESS;
    do{
        std::string name("ccc-ice-");
        name = name + (obj->role == PJ_ICE_SESS_ROLE_CONTROLLING ? "caller" : "callee");
        pjst = pj_ice_sess_create(&obj->ice_cfg->stun_cfg,
                                  name.c_str(), //"ccc-ice",
                                  obj->role,
                                  obj->channelCount,
                                  &ice_cb,
                                  &local_ufrag,
                                  &local_pwd,
                                  obj->grp_lock,
                                  &obj->ice_sess);
        if(pjst != PJ_SUCCESS){
            break;
        }
        obj->ice_sess->user_data = (void*)obj;
        pj_ice_sess_set_options(obj->ice_sess, &obj->ice_cfg->opt);
        
        for(int i = 0; i < obj->channelCount; i++){
            confchannel * ch = &obj->channels[i];
            
//            pj_sockaddr * soaddr = & ch->bound_addr;
            pj_sockaddr * soaddr = & ch->bound_addr_fake;
            
//            pj_str_t fake_ip;
//            pj_str_t fake_remote_ip;
//            if(obj->role == PJ_ICE_SESS_ROLE_CONTROLLING){
//                pj_strdup2_with_null(obj->pool, &fake_ip, CALLER_FAKE_IP);
//                pj_strdup2_with_null(obj->pool, &fake_remote_ip, CALLEE_FAKE_IP);
//            }else{
//                pj_strdup2_with_null(obj->pool, &fake_ip, CALLEE_FAKE_IP);
//                pj_strdup2_with_null(obj->pool, &fake_remote_ip, CALLER_FAKE_IP);
//            }
//            
//            pj_sockaddr_init(pj_AF_INET(), &ch->bound_addr_fake, &fake_ip, 2000+i);
//            pj_sockaddr_init(pj_AF_INET(), &ch->remote_addr_fake, &fake_remote_ip, 2000+i);

            make_addr(obj->pool, CALLER_FAKE_IP, 2000+i, obj->env_af, &ch->bound_addr_fake);
            make_addr(obj->pool, CALLEE_FAKE_IP, 2000+i, obj->env_af, &ch->remote_addr_fake);
            
            
            pj_ice_cand_type candtype = PJ_ICE_CAND_TYPE_HOST;
            pj_str_t founda;
            pj_ice_calc_foundation(obj->pool, &founda, candtype, soaddr);
            pj_sockaddr rel_soaddr;
//            pj_sockaddr_init(pj_AF_INET(), &rel_soaddr, NULL, 0);
             make_addr(obj->pool, NULL, 0, obj->env_af, &rel_soaddr);
            
            pjst = pj_ice_sess_add_cand(obj->ice_sess, ch->comp_id,
                                          TP_STUN, candtype,
                                          HOST_PREF,
                                          &founda, soaddr,
                                          soaddr,  &rel_soaddr,
                                          pj_sockaddr_get_len(soaddr),
                                          &ch->cand_id);
            if (pjst != PJ_SUCCESS){
                dbge("add ice cand fail, comp_id=%d !!!", ch->comp_id);
                break;
            }
            
            
            
            pj_ice_sess_cand * cand = &obj->remote_cands[i];
            memset(cand, 0, sizeof(pj_ice_sess_cand));
            
            cand->comp_id = i+1;
            cand->type = PJ_ICE_CAND_TYPE_HOST;
            cand->prio = 0;
            
            
            pj_str_t str;
            pj_strdup2_with_null(obj->pool, &str, obj->serverIp->c_str());
            //pj_sockaddr_init(pj_AF_INET(), &cand->addr, &str, obj->serverPort+i);  // simulate remote port
            pj_sockaddr_cp(&cand->addr, &ch->remote_addr_fake);
            
            pj_ice_calc_foundation(obj->pool, &cand->foundation, cand->type, &cand->addr);
            
            
            std::string rel_addr = "";
            int rel_port = 0;
            
//            pj_str_t pj_str_reladdr;
//            pj_strdup2_with_null(obj->pool, &pj_str_reladdr, rel_addr.c_str());
//            pj_sockaddr_init(pj_AF_INET(), &cand->rel_addr, &pj_str_reladdr,
//                             rel_port);
            make_addr(obj->pool, rel_addr.c_str(), rel_port, obj->env_af, &cand->rel_addr);
            

        }
        if (pjst != PJ_SUCCESS){
            break;
        }
        
        pj_str_t remote_ufrag;
        pj_str_t remote_pwd;
        pj_strdup2_with_null(obj->pool, &remote_ufrag, obj->str_remote_ufrag->c_str());
        pj_strdup2_with_null(obj->pool, &remote_pwd, obj->str_remote_pwd->c_str());
        
        
        pjst = pj_ice_sess_create_check_list(obj->ice_sess, &remote_ufrag, &remote_pwd,
                                               obj->channelCount, obj->remote_cands);
        
        if (pjst != PJ_SUCCESS){
            dbge("create check list fail!!!");
            break;
        }
        
        
        pjst = pj_ice_sess_start_check(obj->ice_sess);
        if (pjst != PJ_SUCCESS) {
            dbge("start check fail!!!");
            break;
        }
        
        
        _set_state(obj, CONF_OP_NEGOING, 0);
        
        
    }while(0);
    
    
    if(pjst != PJ_SUCCESS){
        _set_final(obj, pjst);
        return pjst;
    }
    return 0;
}

static pj_bool_t on_data_recvfrom(pj_activesock_t *asock,
				  void *data,
				  pj_size_t size,
				  const pj_sockaddr_t *src_addr,
				  int addr_len,
				  pj_status_t status)
{
    confchannel * ch = (confchannel*) pj_activesock_get_user_data(asock);
    confice_t obj = ch->owner;
    pj_uint8_t * buf = (pj_uint8_t *) data;
    pj_uint8_t pkt_type = buf[0];
    pj_uint32_t len = (pj_uint32_t)size;
    
    dbgi("recvfrom bytes %d", size);
    pj_grp_lock_acquire(obj->grp_lock);
    switch(pkt_type)
    {
        case PKT_TYPE_HEARTBEAT:
        break;
            
        case PKT_TYPE_REGISTER:
        {
            if(!_check_reg_ack(obj, ch->channelId, buf, len))
            {
                break;
            }
            
            if(ch->owner->state == CONF_OP_REGING)
            {
                dbgi("got reg ack, comp_id %u", ch->comp_id);
                ch->is_reg = 1;
                int all_ch_reg = 1;
                for(int i = 0; i < obj->channelCount; i++)
                {
                    if(!obj->channels[i].is_reg){
                        all_ch_reg = 0;
                        break;
                    }
                }
                
                if(all_ch_reg){
                    dbgi("all channel reg, start nego");
                    _set_state(obj, CONF_OP_REG_DONE, 0);
                    _start_nego(obj);
                    dbgi("aaaaaa: state=%d", obj->state);
                }
                
            }
        }
        break;
            
        case PKT_TYPE_CTRL:
        break;
            
        case PKT_TYPE_MSG:
            //		case PKT_TYPE_TOKEN_CHANGED:
            //		case PKT_TYPE_TOKEN_RELEASED:
        {
            pj_uint32_t offset = 1;
            pj_uint8_t msg_type = buf[offset];  // msg type
            offset += 1;
            
            pj_uint32_t sess_id = be_get_u32(&buf[offset]); // session id
            offset += 4;
            
            pj_uint32_t comp_id = be_get_u32(&buf[offset]); // comp id
            offset += 4;
            
            pj_uint32_t tsx_id = be_get_u32(&buf[offset]); // transaction id
            offset += 4;
            
            pj_uint32_t data_len = be_get_u32(&buf[offset]); // data length
            offset += 4;
            
            if(sess_id != obj->sessionId){
                dbgi("got packet with session: expect %u but %u", obj->sessionId, sess_id);
                break;
            }
            
            if(comp_id != ch->comp_id){
                dbgi("got packet with comp_id: expect %u but %u", ch->comp_id, comp_id);
                break;
            }
            
            
            if(msg_type == MSG_TYPE_ICE_WRAP){
                if(!obj->ice_sess){
                    dbgi("got early ice wrap packet, drop");
                    break;
                }
                if (!_verify_checksum(buf, len)){
                    dbgi("ice wrap packet checksum error");
                    break;
                }
                
                if(comp_id == 0 || comp_id > obj->channelCount){
                    dbge("got ice packet, wrong comp_id %u", comp_id);
                    break;
                }
                
                int addr_len = sizeof(obj->remote_cands[comp_id-1].addr);
                src_addr = &obj->remote_cands[comp_id-1].addr;
                pj_ice_sess_on_rx_pkt(obj->ice_sess, comp_id,
                                      TP_STUN, &buf[offset], data_len,
                                      src_addr, addr_len);
                
            }
            else if(msg_type == MSG_TYPE_SELECT_RELAY)
            {
                
                if (obj->role != PJ_ICE_SESS_ROLE_CONTROLLED) {
                    dbgi("got select relay but NOT controlled role, drop");
                    break;
                }
                
                if(obj->state < CONF_OP_NEGO_DONE){
                    dbgi("got early select relay packet, drop");
                    break;
                }
                
                if (!_verify_checksum(buf, len)){
                    dbgi("select relay packet checksum error");
                    break;
                }
                
                if(comp_id == 0 || comp_id > obj->channelCount){
                    dbge("got select relay packet, wrong comp_id %u", comp_id);
                    break;
                }
                
                ch->is_relay_done = 1;
                dbgi("conf: got select relay, comp_id=%d", comp_id);
                
                
                // send ack
                pj_memcpy(ch->pkt_common, buf, len-1);
                ch->pkt_common[1] = MSG_TYPE_SELECT_RELAY_ACK;
                ch->pkt_common_len = len -1;
                
                // checksum
                ch->pkt_common[ch->pkt_common_len] = _calc_checksum(ch->pkt_common, ch->pkt_common_len);
                ch->pkt_common_len++;
                
                pj_ssize_t pktsz = ch->pkt_common_len;
                pj_activesock_sendto(ch->active_sock, &ch->int_send_key, ch->pkt_common,
                                     &pktsz, 0, &obj->server_addr,
                                     pj_sockaddr_get_len(&obj->server_addr));
                
                _check_for_select_relay_done(obj);
                
            }
            else if(msg_type == MSG_TYPE_SELECT_RELAY_ACK)
            {
                if (obj->role != PJ_ICE_SESS_ROLE_CONTROLLING) {
                    dbgi("got select relay ack but NOT controlling role, drop");
                    break;
                }
                
                if(obj->state != CONF_OP_SELECTING){
                    dbgi("got select relay ack but wrong state (%d), drop", obj->state);
                    break;
                }
                
                if (!_verify_checksum(buf, len)){
                    dbgi("select relay packet ack checksum error");
                    break;
                }
                
                if(comp_id == 0 || comp_id > obj->channelCount){
                    dbge("got select relay packet, wrong comp_id %u", comp_id);
                    break;
                }
                
                if(tsx_id != ch->tsx_id){
                    dbgi("select relay packet ack tsx_id, expect %u but %u", ch->tsx_id, tsx_id);
                    break;
                }
                
                ch->is_relay_done = 1;
                dbgi("conf: got select relay ack, comp_id=%d", comp_id);
                _check_for_select_relay_done(obj);
            }
            else
            {
                dbgi("unknown msg type 0x02%x\n", msg_type);
            }
        }
        break;
            
        default:
            dbgi("unknown packet type 0x%02x\n", pkt_type);
    }
    pj_grp_lock_release(obj->grp_lock);
    
	return PJ_TRUE; // need to recv more data
}




/* Callback from active socket about send status */
static pj_bool_t on_data_sent(pj_activesock_t *asock,
			      pj_ioqueue_op_key_t *send_key,
			      pj_ssize_t sent)
{
	confchannel * ch;

    ch = (confchannel*) pj_activesock_get_user_data(asock);
    if (!ch) return PJ_FALSE;

    /* Don't report to callback if this is internal message */
    if (send_key == &ch->int_send_key) {
    	return PJ_TRUE;
    }

	return PJ_TRUE; // need to send more data
}

static void _schedule_timer(confice_t obj)
{
    pj_time_val delay;
    delay.sec = 0;
    delay.msec = 200;
    pj_timer_heap_schedule_w_grp_lock(obj->timer_heap,
                                      &obj->timer, &delay, PJ_TRUE, obj->grp_lock);
}

static void timer_cb(pj_timer_heap_t *th, pj_timer_entry *te)
{
	confice_t  obj;

    obj = (confice_t) te->user_data;

    PJ_UNUSED_ARG(th);
    pj_grp_lock_acquire(obj->grp_lock);
    
    pj_timestamp ts;
    pj_get_timestamp (&ts);
    
    if(obj->state == CONF_OP_REGING){
        pj_uint32_t elapsed_ms = pj_elapsed_msec(&obj->reg_send_time, &ts);
        if(elapsed_ms < obj->timeout_ms){
            dbgi("retransmit reg packets");
            _send_reg(obj);
        }else{
            dbge("reg timeout %u", obj->timeout_ms);
            _set_final(obj, -1);
        }
    	
    }
    
    if(obj->state == CONF_OP_SELECTING){
        pj_uint32_t elapsed_ms = pj_elapsed_msec(&obj->sel_relay_send_time, &ts);
        if(elapsed_ms < obj->timeout_ms){
            dbgi("retransmit select relay packets");
            for(unsigned ui = 0; ui < obj->channelCount; ui++){
                confchannel * ch = &obj->channels[ui];
                pj_ssize_t pktsz = ch->pkt_common_len;
                pj_activesock_sendto(ch->active_sock, &ch->int_send_key, ch->pkt_common,
                                     &pktsz, 0, &obj->server_addr,
                                     pj_sockaddr_get_len(&obj->server_addr));
                
            }
            
        }else{
            dbge("select relay timeout %u", obj->timeout_ms);
            _set_final(obj, -1);
        }
        
    }
    
    
    

    if(obj->state != CONF_OP_FINAL){
        _schedule_timer(obj);
    }else{
        dbgi("timer stopped");
    }
    
    pj_grp_lock_release(obj->grp_lock);
}

//static void check_ice_lock(confice_t obj){
//    pj_status_t st = pj_grp_lock_tryacquire(obj->ice_sess->grp_lock);
//    if(st == PJ_SUCCESS){
//        pj_grp_lock_release(obj->ice_sess->grp_lock);
//    }else{
//        char errstr[512];
//        pj_size_t esz = sizeof(errstr);
//        pj_str_t str = pj_strerror	(	st,
//                                     errstr,
//                                     esz
//                                     );
//        dbgi("try lock err: %s", errstr);
//    }
//}

void confice_free(confice_t obj){
	if(!obj) return;
    
    
    pj_timer_heap_cancel_if_active(obj->ice_cfg->stun_cfg.timer_heap, &obj->timer, 0);
    
    
    if (obj->ice_sess) {
//        check_ice_lock(obj);
        pj_ice_sess_destroy(obj->ice_sess);
        obj->ice_sess= NULL;
    }
    
    
    for(int i = 0; i < obj->channelCount; i++){
        confchannel * ch = &obj->channels[i];
        
        if (ch->active_sock != NULL) {
            ch->sock_fd = PJ_INVALID_SOCKET;
            pj_activesock_close(ch->active_sock);
        } else if (ch->sock_fd && ch->sock_fd != PJ_INVALID_SOCKET) {
            pj_sock_close(ch->sock_fd);
            ch->sock_fd = PJ_INVALID_SOCKET;
        }
    }
    
    if(obj->channels){
        free(obj->channels);
        obj->channels = 0;
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
            dbge("release lock return %d, NOT PJ_EGONE !!!", s); 
        }
        obj->grp_lock = 0;
    }
    
    if(obj->confId){
        delete obj->confId;
        obj->confId = 0;
    }
    
    if(obj->serverIp){
        delete obj->serverIp;
        obj->serverIp = 0;
    }
    
    if(obj->rcode){
        delete obj->rcode;
        obj->rcode = 0;
    }
    
    
	free(obj);
}

void * confice_userdata(confice_t obj){
    return obj->user_data;
}

int confice_new(Json_em::Value& config
                , pj_ice_strans_cfg * ice_cfg
                , pj_pool_t * pool
                , int env_af
//                , pj_ioqueue_t * ioqueue
//                , pj_timer_heap_t	*timer_heap
                , int role
                , std::string& local_ufrag
                , std::string& local_pwd
                , std::string& remote_ufrag
                , std::string& remote_pwd
                , const confice_cb * cb
                , void * user_data
                , confice_t * pobj) {

	int ret = -1;
	confice_t obj = 0;

    pj_stun_config * stun_cfg = &ice_cfg->stun_cfg;
    pj_ioqueue_t * ioqueue = stun_cfg->ioqueue;
    pj_timer_heap_t	*timer_heap = stun_cfg->timer_heap;
    
	do{
		obj = (confice_t) malloc(sizeof(struct tag_confice));
        memset(obj, 0, sizeof(struct tag_confice));

        pj_ansi_snprintf(obj->obj_name, sizeof(obj->obj_name), "cice%p", obj);
        pj_memcpy(&obj->cb, cb, sizeof(*cb));
        obj->user_data = user_data;
        
		obj->pool = pool;
		obj->ioqueue = ioqueue;
		obj->timer_heap = timer_heap;
        obj->ice_cfg = ice_cfg;
        obj->role = (pj_ice_sess_role)role;

		ret = pj_grp_lock_create(obj->pool, NULL, &obj->grp_lock);
		if (ret != PJ_SUCCESS) {
			dbge("confice: create lock fail");
			break;
		}

        pj_grp_lock_add_ref(obj->grp_lock);
//        pj_grp_lock_add_handler(obj->grp_lock, pool, obj, &aaaa_func);
        
        
		obj->timer.cb = &timer_cb;
		obj->timer.user_data = obj;


		// parse config
		//"u1" : {
		//      "conferenceId" : "voicep2p-8c4eeb0d-0a45-4fe5-a3c2-5ab146e6dba2",
		//      "serverIp" : "10.0.1.161",
		//      "rcode" : "2027467697",
		//      "serverPort" : 5000,
		//      "channelId" : 1,
		//		"vchannelId" : 3
		//    },
        
        dbgi("confice parse config, role=%d ...", obj->role);
		obj->confId = new std::string(config["conferenceId"].asString());
		obj->serverIp = new std::string(config["serverIp"].asString());
		obj->rcode = new std::string(config["rcode"].asString());
		obj->serverPort = config["serverPort"].asInt();
        obj->sessionId = config["sessionId"].asInt();
        
        obj->str_local_ufrag = new std::string(local_ufrag.c_str());
        obj->str_local_pwd = new std::string(local_pwd.c_str());
        obj->str_remote_ufrag = new std::string(remote_ufrag.c_str());
        obj->str_remote_pwd = new std::string(remote_pwd.c_str());
        
        obj->timeout_ms = CICE_DEFAULT_TIMEOUT_MS;
        
        dbgi("confice parse config, role=%d done", obj->role);
		obj->channelCount = 0;

//		int af = pj_AF_INET();
//		pj_sockaddr * saddr = &obj->server_addr;
//		pj_str_t strServerIp = pj_str((char *) obj->serverIp->c_str());
//
//		saddr->addr.sa_family = (pj_uint16_t) af; 
//        
//        const pj_sockaddr *sa = (const pj_sockaddr*)saddr;
//        dbgi("af=%d", af);
//        dbgi("sa->addr.sa_family=%d", sa->addr.sa_family);
//        
//        void * a = pj_sockaddr_get_addr(saddr);
//		ret = pj_inet_pton(af, &strServerIp, a);
//		if (ret != PJ_SUCCESS) {
//			dbge("confice: init server addr fail !!!");
//			break;
//		}
//		pj_sockaddr_set_port(saddr, obj->serverPort);
        
        // for ipv6
        int af = env_af;
        obj->env_af = env_af;
        char ipv6[128];
        make_addr_v2(obj->pool, obj->serverIp->c_str(), obj->serverPort, env_af, &obj->server_addr, ipv6);
        if(ipv6[0]){
            dbgi("conference serverIp ipv6: %s -> %s", obj->serverIp->c_str(), ipv6);
            delete obj->serverIp;
            obj->serverIp = new std::string(ipv6);
        }
        

		if(config["vchannelId"].empty()){
			obj->channelCount = 1;
			size_t sz = obj->channelCount * sizeof(confchannel);
			obj->channels = (confchannel *)malloc(sz);
			memset(obj->channels, 0, sz);
			obj->channels[0].channelId = config["channelId"].asInt();

		}else{
			obj->channelCount = 2;
			size_t sz = obj->channelCount * sizeof(confchannel);
			obj->channels = (confchannel*)malloc(sz);
			memset(obj->channels, 0, sz);
			obj->channels[0].channelId = config["channelId"].asInt();
			obj->channels[1].channelId = config["vchannelId"].asInt();
		}

		for(int i = 0; i < obj->channelCount; i++){
			confchannel * ch = &obj->channels[i];
			ch->owner = obj;
            ch->comp_id = i+1;
			ch->sock_fd = PJ_INVALID_SOCKET;
			pj_ioqueue_op_key_init(&ch->int_send_key, sizeof(ch->int_send_key));


			ret = pj_sock_socket(af, pj_SOCK_DGRAM(), 0, &ch->sock_fd);
			if (ret != PJ_SUCCESS) {
				dbge("create socket fail, channelId %d!!!", ch->channelId);
				break;
			}

			int reuseaddr_on = 1;
			if (setsockopt(ch->sock_fd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr_on, sizeof(reuseaddr_on)) == -1)
			{
				dbge("fail to set socket reuse, fd: %d", ch->sock_fd);
			}

			pj_uint16_t	port_range = 0;
			pj_uint16_t max_bind_retry = 100;
			pj_sockaddr_init(af, &ch->bound_addr, NULL, 0);
			ret = pj_sock_bind_random(ch->sock_fd, &ch->bound_addr,
							 port_range, max_bind_retry);
			if (ret != PJ_SUCCESS){
				dbge("bind socket fail, channelId %d!!!", ch->channelId);
				break;
			}

			int addr_len = sizeof(ch->bound_addr);
			ret = pj_sock_getsockname(ch->sock_fd, &ch->bound_addr,
					&addr_len);
			if (ret != PJ_SUCCESS) {
				dbge("get socket sock name fail, channelId %d!!!", ch->channelId);
				break;
			}


			pj_activesock_cfg activesock_cfg;
			pj_activesock_cfg_default (&activesock_cfg);
			activesock_cfg.grp_lock = obj->grp_lock;
			activesock_cfg.async_cnt = 1;
			activesock_cfg.concurrency = 0;

			pj_activesock_cb activesock_cb;
			pj_bzero(&activesock_cb, sizeof(activesock_cb));
			activesock_cb.on_data_recvfrom = &on_data_recvfrom;
			activesock_cb.on_data_sent = &on_data_sent;

			ret = pj_activesock_create(obj->pool, ch->sock_fd,
			pj_SOCK_DGRAM(), &activesock_cfg, obj->ioqueue, &activesock_cb,
					ch, &ch->active_sock);
			if (ret != PJ_SUCCESS){
				dbge("create active sock fail, channelId %d!!!", ch->channelId);
				break;
			}

			ret = pj_activesock_start_recvfrom(ch->active_sock, pool, 2*1024, 0);
			if (ret != PJ_SUCCESS) {
				dbge("start recv from fail, channelId %d!!!", ch->channelId);
				break;
			}

		}
		if(ret) break;

         _schedule_timer(obj);
        
		*pobj = obj;
		ret = 0;
	}while(0);

	if(ret != 0){
		confice_free(obj);
		obj = 0;
	}
	return ret;
}



int confice_kickoff_reg(confice_t obj) {
	int ret = -1;

	pj_grp_lock_acquire(obj->grp_lock);
	do{
		if(obj->state >= CONF_OP_REGING){
			dbge("confice: already kicked reg !!!");
			ret = -1;
			break;
		}
		dbgi("kicking reg");


		_send_reg(obj);
		
		
		ret = 0;

	}while(0);
	pj_grp_lock_release(obj->grp_lock);

	return ret;
}


int confice_kickoff_select_relay(confice_t obj) {
    int ret = -1;
    
    pj_grp_lock_acquire(obj->grp_lock);
    do{
        if(obj->state >= CONF_OP_SELECTING){
            dbge("confice: already kicked select relay !!!");
            ret = -1;
            break;
        }
        dbgi("kicking select relay");
        obj->sel_relay_kicked = 1;
        
        bool b = _check_for_select_relay(obj);
        dbgi("conf-ice: kick-select-relay  %s", b ? "true" : "false");
        
        
        ret = 0;
        
    }while(0);
    pj_grp_lock_release(obj->grp_lock);
    
    return ret;
}

int confice_cancel(confice_t obj) {
    int ret = -1;
    
    pj_grp_lock_acquire(obj->grp_lock);
    do{
        if(obj->state >= CONF_OP_FINAL){
            dbge("confice: already final, cancel !!!");
            ret = -1;
            break;
        }
        dbgi("confice: cancelling");
        _set_final(obj, -1);
        
        ret = 0;
        
    }while(0);
    pj_grp_lock_release(obj->grp_lock);
    
    return ret;
}

int confice_steal_fds(confice_t obj, pj_sock_t fds[], int * pnum) {
    if(obj->state != CONF_OP_FINAL){
        return -1;
    }
    if(obj->final_result != 0){
        return -1;
    }

    int num = 0;
    for(int i = 0; i < obj->channelCount; i++){
    	confchannel * ch = &obj->channels[i];
    	if (ch->active_sock != NULL) {
    		fds[num] = pj_activesock_steal_fd(ch->active_sock);
    		num++;
    	}
    }
    *pnum = num;
    return 0;
}

int confice_get_result(confice_t obj, Json_em::Value * p_pairs_val) {
    if(obj->state != CONF_OP_FINAL){
        return -1;
    }
    if(obj->final_result != 0){
        return -1;
    }
    
	Json_em::Value& pairs_val = *p_pairs_val;
	for(int i = 0; i < obj->channelCount; i++){
		confchannel * ch = &obj->channels[i];
		int comp_id = (i+1);
		Json_em::Value remote_val;

		remote_val["component"] = comp_id;
		remote_val["type"] = "relayMS";

//		remote_val["serverIp"] = obj->serverIp->c_str();
//		remote_val["serverPort"] = obj->serverPort;
		remote_val["ip"] = obj->serverIp->c_str();
		remote_val["port"] = obj->serverPort;
		remote_val["protocol"] = "udp";

		remote_val["conferenceId"] = obj->confId->c_str();
		remote_val["rcode"] = obj->rcode->c_str();
		remote_val["channelId"] = ch->channelId;

		Json_em::Value local_val;
		char ip[MAX_IP_SIZE];
		int port;
//        get_ip_port_from_sockaddr(&ch->bound_addr, ip, &port);
        port = (int) pj_sockaddr_get_port(&ch->bound_addr);
        if(pj_sockaddr_has_addr(&ch->bound_addr)){
            pj_sockaddr_print(&ch->bound_addr, ip, MAX_IP_SIZE, 0);
        }else{
            strcpy(ip, "0.0.0.0");
        }
		
		local_val["component"] = comp_id;
		local_val["type"] = "host";
		local_val["ip"] = ip;
		local_val["port"] = port;
		local_val["protocol"] = "udp";


		Json_em::Value pair_val;
		pair_val["comp_id"] = comp_id;
		pair_val["local"] = local_val;
		pair_val["remote"] = remote_val;
		pairs_val.append(pair_val);
	}
	return 0;
}


#define KEEP_ALIVE_INTERVAL 300
#define MAX_COMPONENT_COUNT 4
#define MAX_CANDIDATE_COUNT 8


struct eice_context{
	int pj_inited;
	int pjlib_inited;
	int pjnath_inited;
	int pjlog_installed;
	FILE *log_fhnd;
    
    pj_caching_pool cp;
    int cp_inited;
    pj_pool_t * pool;

};


class eice_config{
public:
	std::string turn_host; // default turn server
	int turn_port;
    
//    std::string turn_all_hosts[PJ_STUN_SOCK_MAX_STUN_NUM]; // all turn servers
//    int turn_all_port[PJ_STUN_SOCK_MAX_STUN_NUM];
//    int turn_num;
    
    stun_bind_cfg bind_cfg;
	int comp_count;

	Json_em::Value relay_ms;
	Json_em::Value relay_caller;
	Json_em::Value relay_callee;
    
    Json_em::Value turnAddrs;
};



//#define DEBUG_TEST
#ifdef DEBUG_TEST
static int get_streal_start(pj_ice_sess_role role){
	if (role == PJ_ICE_SESS_ROLE_CONTROLLING) return 0;
	return MAX_STEAL_SOCKET / 2;
}
static int get_streal_end(pj_ice_sess_role role){
	if (role == PJ_ICE_SESS_ROLE_CONTROLLING) return MAX_STEAL_SOCKET / 2;
	return MAX_STEAL_SOCKET;
}

#else
#define get_streal_start(x)  0
#define get_streal_end(x) MAX_STEAL_SOCKET
#endif

struct eice_st{
    char		obj_name[PJ_MAX_OBJ_NAME];
    
	eice_config * cfg;

    // ice instance
    pj_caching_pool cp;
    int cp_inited;

    pj_pool_t *ice_pool;


    pj_lock_t	    *lock;
    pj_grp_lock_t * grp_lock;

    pj_ice_strans_cfg ice_cfg;
    pj_ice_strans *icest;
    pj_ice_sess_role role;

    int ice_init_done;
    pj_status_t ice_init_result;

    int ice_nego_done;
    pj_status_t ice_nego_status;


    // callback thread.
    pj_thread_t *ice_thread;
    int thread_quit_flag;
    int thread_exited_flag;

	int ioq_stop_req;
	int ioq_stopped;


//    char local_ufrag[512];
//    char local_pwd[512];

    std::string * local_content;
    std::string * remote_content;

    pj_str_t rufrag;
	pj_str_t rpwd;
    pj_ice_sess_cand remote_cands[MAX_CANDIDATE_COUNT];
    int remote_cand_count;
    std::string * remote_turn;
    
    char err_str[1024];

    confice_t confice;
    std::string * str_local_ufrag;
    std::string * str_local_pwd;
    std::string * str_remote_ufrag;
    std::string * str_remote_pwd;
    int confice_done;
    pj_status_t confice_status;
    

	#define MAX_STEAL_SOCKET 8
	pj_sock_t g_steal_sockets[MAX_STEAL_SOCKET];
	int g_steal_socket_count;
//	pj_grp_lock_t * g_steal_lock;

    
    pj_timer_entry	 timer; // nego timeout timer
    
    int force_relay;
    int remote_force_relay;
		

	void init_steal_sockets(pj_pool_t * pool);
	void close_steal_sockets();
	void update_steal_sockets();
	int eice_get_global_socket(int port);
};



static struct eice_context g_eice_stor;
static struct eice_context * g_eice = 0;

static int eice_worker_thread(void *arg);
static pj_status_t handle_events(eice_t obj, unsigned max_msec, unsigned *p_count);

//#define REGISTER_THREAD pj_thread_t *pthread = NULL; \
//pj_thread_desc desc; \
//pj_bool_t has_registered = PJ_FALSE; \
//has_registered = pj_thread_is_registered(); \
//if(!has_registered) { \
//if (pj_thread_register(NULL, desc, &pthread) == PJ_SUCCESS) { \
//} \
//}

static void register_eice_thread(pj_pool_t * pool){
    pj_thread_t *pthread = NULL;
//    pj_thread_desc desc;
    pj_thread_desc * pdesc ;
    pj_bool_t has_registered = PJ_FALSE;
    has_registered = pj_thread_is_registered();
    if(has_registered) return;
    
    pj_size_t sz =  sizeof(pj_thread_desc);
    pdesc = (pj_thread_desc *)pj_pool_alloc(pool,sz);
    
    if (pj_thread_register(NULL, *pdesc, &pthread) != PJ_SUCCESS) {
        dbg(" **** register thread ERROR ****");
    }
}

//PJ_DEF(pj_status_t) pj_thread_auto_register(void)
static pj_status_t auto_register_eice_thread()
{
    pj_status_t rc;
    
    if(!pj_thread_is_registered())
    {
        pj_thread_desc *p_thread_desc;
        pj_thread_t* thread_ptr;
        p_thread_desc = (pj_thread_desc *) calloc(1, sizeof(pj_thread_desc));
        rc = pj_thread_register("ethrd_%p", *p_thread_desc, &thread_ptr);
    }
    else
    {
        rc = PJ_SUCCESS;
    }
    return rc;
}



static void log_func_4pj(int level, const char *data, int len) {

//    register_eice_thread(g_eice->pool);
    
    if(!g_eice) return;

    //pj_log_write(level, data, len);
    printf("%s", data);
}


#define dbgraw(x) printf(x "\n")

int eice_init()
{
    
    int ret = -1;
    dbgraw("eice_init");
    
	if (g_eice) {
		ret = 0;
		dbgraw("eice is already initialized, return ok directly.");
		return 0;
	}

   
    
	g_eice = &g_eice_stor;
	memset(g_eice, 0, sizeof(struct eice_context));
    dbgraw("memset OK");


    do {
        
        ret = pj_init();
        if (ret != PJ_SUCCESS) {
            dbgraw("pj_init failure ");
            break;
        }
        g_eice->pj_inited = 1;
        dbgraw("pj_init OK");
        
        ret = auto_register_eice_thread();
        if (ret != PJ_SUCCESS) {
            dbgraw("auto reg pj thread failure ");
            break;
        }
        dbgraw("auto reg pj thread OK");
        
        // create pool factory
        pj_caching_pool_init(&g_eice->cp, NULL, 0);
        g_eice->cp_inited = 1;
        dbgraw("pj_caching_pool_init OK");
        
        ret = auto_register_eice_thread();
        if (ret != PJ_SUCCESS) {
            dbgraw("auto reg pj thread 2 failure ");
            break;
        }
        dbgraw("auto reg pj thread 2 OK");
        
        /* Create application memory pool */
        g_eice->pool = pj_pool_create(&g_eice->cp.factory, "eice_global_pool",
                                      512, 512, NULL);
        dbgraw("pj_pool_create OK");
        
        register_eice_thread(g_eice->pool);
        dbgraw("register_eice_thread  OK");
        
        pj_log_set_log_func(&log_func_4pj);
        dbgraw("pj_log_set_level OK");
        
        pj_log_set_level(PJ_LOG_MAX_LEVEL);
        dbgraw("pj_log_set_level OK");
        
        
        ret = pjlib_util_init();
        if (ret != PJ_SUCCESS) {
            dbg("pjlib_util_init failure, ret=%d", ret);
            break;
        }
        g_eice->pjlib_inited = 1;
        dbgraw("pjlib_util_init OK");
        
        ret = pjnath_init();
        if (ret != PJ_SUCCESS) {
            dbg("pjnath_init failure, ret=%d", ret);
            break;
        }
        g_eice->pjnath_inited = 1;
        dbgraw("pjnath_init OK");
        

//        init_steal_sockets(g_eice->pool);
        
        ret = PJ_SUCCESS;
        dbgraw("eice init ok");

    } while (0);

    if(ret != PJ_SUCCESS){
    	eice_exit();
    }
    return ret;
}

void eice_exit() {

	if(!g_eice) return;
    
//    if(g_eice->pool){
//        register_eice_thread(g_eice->pool);
//    }
    
    if(g_eice->cp_inited){
        pj_caching_pool_destroy(&g_eice->cp);
        g_eice->cp_inited = 0;
    }
    

	if(g_eice->pj_inited){
		pj_shutdown();
		g_eice->pj_inited = 0;
	}

	dbgraw("eice exit ok");
	g_eice = 0;
}

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

static
const Json_em::Value& _json_get_obj(Json_em::Value& request, const char * name, const Json_em::Value& default_value)
{
    if(request[name].isNull())
    {
        dbg("name %s NOT found in json!!!", name);
        return default_value;
    }
    
    return request[name];
}


static int parse_turn_config(eice_t obj, Json_em::Value& turnAddrs, stun_bind_cfg& bind_cfg){
    bind_cfg.stun_serv_num = 0;
    if (!turnAddrs.empty()) {
        dbgi("turn addr num %d", turnAddrs.size());
        for(unsigned int ui = 0; ui < turnAddrs.size(); ui++){
            Json_em::Value json_addr_value = turnAddrs[ui];
            std::string host = _json_get_string(json_addr_value, "host", std::string(""));
            int port = _json_get_int(json_addr_value, "port", 3478);
            //                cfg->turn_all_hosts[ui] = host;
            //                cfg->turn_all_port[ui] = port;
            pj_strdup2_with_null(obj->ice_pool, &bind_cfg.stun_serv_ips[ui], host.c_str());
            bind_cfg.stun_serv_ports[ui] = port;
        }
        //            cfg->turn_num = turnAddrs.size();
        bind_cfg.stun_serv_num = turnAddrs.size();
    }
    return 0;
}

static eice_config * parse_eice_config(eice_t obj, const char * config_json){
	int ret = -1;
	eice_config * cfg = new eice_config;
	Json_em::Reader reader;
	Json_em::Value value;

	do {
		if(!config_json){
			config_json = "{}";
		}

		if (!reader.parse(config_json, value)) {
			dbge("parse config JSON: %s, fail!!!", config_json);
			ret = -1;
			break;
		}

		cfg->turn_host = _json_get_string(value, "turnHost", std::string(""));
		cfg->turn_port = _json_get_int(value, "turnPort", 3478);
		cfg->comp_count = _json_get_int(value, "compCount", 2);
		if (!value["relayMS"].isNull()) {
			cfg->relay_ms = value["relayMS"];
            pj_timestamp t;
            pj_get_timestamp(&t);
            pj_uint32_t sessionId = t.u32.lo%1000000;
            dbgi("generate sessionId %u", sessionId);
			if (!cfg->relay_ms["caller"].isNull()) {
				cfg->relay_caller = cfg->relay_ms["caller"];
                cfg->relay_caller["sessionId"] = sessionId;
                cfg->relay_ms["caller"] = cfg->relay_caller;
			}

			if (!cfg->relay_ms["callee"].isNull()) {
				cfg->relay_callee = cfg->relay_ms["callee"];
                cfg->relay_callee["sessionId"] = sessionId;
                cfg->relay_ms["callee"] = cfg->relay_callee;
			}
		}


//        cfg->turn_num = 0;
        cfg->bind_cfg.stun_serv_num = 0;
        cfg->bind_cfg.stun_bind_timeout = 3000;
        cfg->turnAddrs = _json_get_array(value, "turnAddrs",Json_em::Value());
        ret = parse_turn_config(obj, cfg->turnAddrs, cfg->bind_cfg);
        if(ret < 0){
            dbge("parse turn config fail!!!");
            break;
        }
        ret = -1;
        
        // use serv[0] as default
        if(cfg->turn_host.empty() && cfg->turnAddrs.size() > 0){
            cfg->turn_host = std::string(cfg->bind_cfg.stun_serv_ips[0].ptr, cfg->bind_cfg.stun_serv_ips[0].slen);
            cfg->turn_port = cfg->bind_cfg.stun_serv_ports[0];
        }
        


        ret = 0;
	} while (0);

	if(ret != 0){
		delete cfg;
		cfg = 0;
	}
	return cfg;
}

static void dump_eice_config(eice_t obj, eice_config * cfg){
    dbgi("=== eice_config ===>");
	dbgi("defalut turn server: %s:%d", cfg->turn_host.c_str(), cfg->turn_port);
	dbgi("compCount: %d", cfg->comp_count);
    dbgi("turnAddrs num: %d", cfg->bind_cfg.stun_serv_num);
    for(int i = 0; i < cfg->bind_cfg.stun_serv_num; i++){
        dbgi("turnAddrs[%d]: %s:%d", i, cfg->bind_cfg.stun_serv_ips[i].ptr, cfg->bind_cfg.stun_serv_ports[i]);
    }
    dbgi("<=== eice_config ===");
}


static void cb_on_ice_complete(pj_ice_strans *ice_st, pj_ice_strans_op op,
		pj_status_t status) {

    eice_t obj = (eice_t) pj_ice_strans_get_user_data(ice_st);
//    register_eice_thread(g_eice->pool);

    dbge("cb_on_ice_complete: op=%d", op);
    

	switch (op) {
	case PJ_ICE_STRANS_OP_INIT: {
		dbgi("ice init result : %s", status == PJ_SUCCESS ? "OK" : "FAIL");
		pj_lock_acquire(obj->lock);
        if(!obj->ice_init_done){
            obj->ice_init_done = 1;
            obj->ice_init_result = status;
        }
		pj_lock_release(obj->lock);
	}
		break;

	case PJ_ICE_STRANS_OP_NEGOTIATION: {
        dbgi("ice nego result : %s", status == PJ_SUCCESS ? "OK" : "FAIL");
        pj_timer_heap_cancel_if_active(obj->ice_cfg.stun_cfg.timer_heap, &obj->timer, 0);
        int done = 0;
		pj_lock_acquire(obj->lock);
        if(!obj->ice_nego_done){
            obj->ice_nego_done = 1;
            obj->ice_nego_status = status;
            done = 1;
        }
		pj_lock_release(obj->lock);
        
        
        if (done && obj->confice) {
            if (status == PJ_SUCCESS) {
                confice_cancel(obj->confice); // cancel confice for callback the final result
            }else if (obj->role == PJ_ICE_SESS_ROLE_CONTROLLING) {
                confice_kickoff_select_relay(obj->confice);
            }
        }

		// tell user to get ice result
		if (g_nego_result_func != NULL) {
			g_nego_result_func(obj);
		}
	}
		break;
	default: {
		dbge("unknown operation:%d", op);
	}
		break;
	} // end of switch
}

static void on_confice_complete(confice_t cice, confice_op op, int status){
    eice_t obj = (eice_t) confice_userdata(cice);
    dbgi("on_confice_complete: op %d, status %d", op, status);
    pj_lock_acquire(obj->lock);
    if(op == CONF_OP_FINAL){
        obj->confice_done = 1;
        obj->confice_status = status;
        
    }
    pj_lock_release(obj->lock);
}


static int query_wait(eice_t obj, pj_lock_t * lock, int * pflag, int * pcancel, unsigned int timeout_ms){

	pj_timestamp ts_start;
	pj_timestamp ts;
	int ret = -1;
	int cancel =  0;

	dbgi("before pcancel=%p", pcancel);
	if(!pcancel) {
		pcancel = &cancel;
	}
	dbgi("after pcancel=%p", pcancel);

	pj_lock_acquire(lock);
	pj_get_timestamp(&ts_start);
	ts = ts_start;
	while (!(*pflag) && !(*pcancel)) {
//		unsigned int elapse = (unsigned ) (ts.u64 - ts_start.u64);
		pj_uint32_t elapse = pj_elapsed_msec(&ts_start, &ts);
		dbgi("elapsed=%d msec", elapse);
		if(elapse >= timeout_ms) break;
        pj_lock_release(lock);
        
		pj_thread_sleep(50);
		pj_get_timestamp(&ts);
		pj_lock_acquire(lock);
	}
	dbgi("(*pflag)=%d", (*pflag));
	ret = (*pflag) != 0 ? 0 : -1;
	pj_lock_release(lock);
	return ret;
}



static void cand_to_json_value(int comp_id, int cand_id, const pj_ice_sess_cand * cand, Json_em::Value& json_val_cand){
	char ip[MAX_IP_SIZE];
	int port;
	char base_ip[MAX_IP_SIZE];
	int base_port;

	get_ip_port_from_sockaddr(&cand->addr, ip, &port);
	get_ip_port_from_sockaddr(&cand->base_addr, base_ip, &base_port);
//	dbgi("\t cand[%d,%d]: addr=%s:%d, base_addr=%s:%d", comp_id, cand_id, ip, port, base_ip, base_port);

	json_val_cand["component"] = cand->comp_id;
	json_val_cand["foundation"] = std::string(cand->foundation.ptr,
			cand->foundation.slen);

	json_val_cand["generation"] = "0";
	json_val_cand["network"] = "1";

	json_val_cand["id"] = cand_id; // to be check
	json_val_cand["ip"] = ip;
	json_val_cand["port"] = port;
	json_val_cand["priority"] = cand->prio;

	json_val_cand["protocol"] = "udp";
	json_val_cand["type"] = pj_ice_get_cand_type_name(cand->type);
	if (cand->type != PJ_ICE_CAND_TYPE_HOST) {
		if (pj_sockaddr_has_addr(&cand->base_addr)) {
			json_val_cand["rel-addr"] = base_ip;
			json_val_cand["rel-port"] = base_port;
		} else {
			json_val_cand["rel-addr"] = ip;
			json_val_cand["rel-port"] = port;
		}
	}
}


static void local_cand_to_json_value(int comp_id, int cand_id, const pj_ice_sess_cand * cand, Json_em::Value& json_val_cand){
    char ip[MAX_IP_SIZE];
    int port;
    char base_ip[MAX_IP_SIZE];
    int base_port;
    
    get_ip_port_from_sockaddr(&cand->addr, ip, &port);
    get_ip_port_from_sockaddr(&cand->base_addr, base_ip, &base_port);
    //	dbgi("\t cand[%d,%d]: addr=%s:%d, base_addr=%s:%d", comp_id, cand_id, ip, port, base_ip, base_port);
    
    json_val_cand["component"] = cand->comp_id;
    json_val_cand["foundation"] = std::string(cand->foundation.ptr,
                                              cand->foundation.slen);
    
    json_val_cand["generation"] = "0";
    json_val_cand["network"] = "1";
    
    json_val_cand["id"] = cand_id; // to be check
    json_val_cand["ip"] = base_ip;
    json_val_cand["port"] = base_port;
    json_val_cand["priority"] = cand->prio;
    
    json_val_cand["protocol"] = "udp";
    json_val_cand["type"] = pj_ice_get_cand_type_name(cand->type);
    if (cand->type != PJ_ICE_CAND_TYPE_HOST) {
        if (pj_sockaddr_has_addr(&cand->base_addr)) {
            json_val_cand["rel-addr"] = base_ip;
            json_val_cand["rel-port"] = base_port;
        } else {
            json_val_cand["rel-addr"] = ip;
            json_val_cand["rel-port"] = port;
        }
    }
}




static int _get_local(eice_t obj){
	int ret = 0;
	do{
        Json_em::Value json_val;
        json_val.clear();
        
        
        if(obj->icest){
            
            if(!obj->ice_init_done) break;
            
            // create ice session after ice strans inited;
            ret = pj_ice_strans_init_ice(obj->icest, obj->role, 0, 0);
            if (ret != PJ_SUCCESS) {
                dbge("error init ice session, ret=%d", ret);
                break;
            }
            
            //		dbgi("sleeping 2000 ms");
            //		pj_thread_sleep(2000);
            
            // get local ufrag and pwd
            
            pj_str_t loc_ufrag;
            pj_str_t loc_pwd;
            ret = pj_ice_strans_get_ufrag_pwd(obj->icest, &loc_ufrag, &loc_pwd, NULL,
                                              NULL); // ignore remote.
            if (ret != PJ_SUCCESS) {
                dbge("error get ufrag and pwd, ret=%d", ret);
                break;
            }
            
            //		strncpy(obj->local_ufrag, loc_ufrag.ptr, loc_ufrag.slen);
            //		obj->local_ufrag[loc_ufrag.slen] = 0;
            //		strncpy(obj->local_pwd, loc_pwd.ptr, loc_pwd.slen);
            //		obj->local_pwd[loc_pwd.slen] = 0;
            //		dbgi("local ufrag: %s", obj->local_ufrag);
            //		dbgi("local pwd: %s", obj->local_pwd);
            
            obj->str_local_ufrag =  new std::string(loc_ufrag.ptr, loc_ufrag.slen);
            obj->str_local_pwd = new std::string(loc_pwd.ptr, loc_pwd.slen);
            dbgi("local ufrag: %s, len=%d", obj->str_local_ufrag->c_str(), loc_ufrag.slen);
            dbgi("local pwd: %s, len=%d", obj->str_local_pwd->c_str(), loc_pwd.slen);
            
            // get local candidates
            Json_em::Value json_val_cands;
            ret = PJ_SUCCESS;
            for (int comp = 1; comp <= obj->cfg->comp_count; comp++) {
                unsigned candCount = MAX_CANDIDATE_COUNT;
                pj_ice_sess_cand cands[MAX_CANDIDATE_COUNT];
                memset(cands, 0, sizeof(pj_ice_sess_cand) * MAX_CANDIDATE_COUNT);
                
                ret = pj_ice_strans_enum_cands(obj->icest, comp, &candCount, cands);
                if (ret != PJ_SUCCESS) {
                    dbge("error get candidates of comp %d", comp);
                    break;
                }
                dbgi("comp %d has candidates %d", comp, candCount);
                
                for (int ci = 0; ci < candCount; ci++) {
                    pj_ice_sess_cand * cand = &cands[ci];
                    
                    // make candidate JSON
                    Json_em::Value json_val_cand;
                    cand_to_json_value(comp, ci, cand, json_val_cand);
                    
                    json_val_cands.append(json_val_cand);
                    
                }
            }
            if (ret != PJ_SUCCESS)
                break;
            
            
            json_val["ufrag"] = *obj->str_local_ufrag;
            json_val["pwd"] = *obj->str_local_pwd;
            json_val["candidates"] = json_val_cands;
            
            
        }
		

		if (!obj->cfg->relay_ms.isNull()) {
			json_val["relayMS"] = obj->cfg->relay_ms;
		}

        if (!obj->cfg->turnAddrs.isNull()){
            json_val["turnAddrs"] = obj->cfg->turnAddrs;
        }
        
        if(obj->force_relay){
            json_val["forceRelay"] = obj->force_relay;
        }
        
        if(!json_val.isNull()){
            Json_em::FastWriter writer;
            obj->local_content = new std::string(writer.write(json_val));
        }else{
            obj->local_content = new std::string("{}");
        }
		

		dbgi("local-content= %s\n", obj->local_content->c_str());
	}while(0);

	return ret;
}

static void timer_ice_cb(pj_timer_heap_t *th, pj_timer_entry *te)
{
    eice_t  obj;
    
    obj = (eice_t) te->user_data;
    
    PJ_UNUSED_ARG(th);
    pj_lock_acquire(obj->lock);
    
    
    dbge("nego timeout %u, stop ice", ICE_TIMEOUT_MS);
    if(obj->icest){
        if (pj_ice_strans_has_sess(obj->icest)) {
            pj_ice_strans_stop_ice(obj->icest);
        }
    }
    
    
    if(!obj->ice_nego_done){
        obj->ice_nego_done = 1;
        obj->ice_nego_status = -1;
        
        if(obj->confice && obj->role == PJ_ICE_SESS_ROLE_CONTROLLING){
            confice_kickoff_select_relay(obj->confice);
        }
        
    }
    
    
    pj_lock_release(obj->lock);
}


static void dump_cand(eice_t obj,  pj_ice_sess_cand * cand, const char * prefix){
    char ip[MAX_IP_SIZE];
    int port;
    char rel_ip[MAX_IP_SIZE];
    int rel_port;
    
    get_ip_port_from_sockaddr(&cand->addr, ip, &port);
    get_ip_port_from_sockaddr(&cand->rel_addr, rel_ip, &rel_port);
    
    dbgi("%s: comp_id=%d, foundation=%s, prio=%d, typ=%s, addr=%s:%d, rel-addr=%s:%d"
         , prefix, cand->comp_id, cand->foundation.ptr, cand->prio, pj_ice_get_cand_type_name(cand->type)
         , ip, port, rel_ip, rel_port);
}

static int parse_content(eice_t obj, const char * content, int content_len
                         , std::string& ufrag, std::string& pwd
                         , pj_ice_sess_cand cands[], int& cand_count){
    int ret = -1;
    do{
        Json_em::Reader reader;
        Json_em::Value value;
        if (!reader.parse(std::string(content, content_len),
                          value)) {
            dbge("parse content JSON fail!!!\n");
            ret = -1;
            break;
        }
        ret = 0;
        ufrag = _json_get_string(value, "ufrag", std::string(""));
        pwd = _json_get_string(value, "pwd", std::string(""));
        Json_em::Value candidates = _json_get_array(value, "candidates",
                                                 Json_em::Value());
        
        
        cand_count = 0;
        if(!ufrag.empty()
           //&& !pwd.empty()
           && !candidates.empty()){
            if (ufrag.empty()) {
                dbge("ufrag empty !!!\n");
                ret = -1;
            }
            if (pwd.empty()) {
                dbgw("pwd empty !!!\n");
                //			ret = -1;
            }
            if (candidates.empty()) {
                dbge("candidates empty !!!\n");
                ret = -1;
            }
            if (ret != 0)
                break;
            
            for (int i = 0; i < candidates.size(); i++) {
                Json_em::Value json_val_cand = candidates[i];
                pj_ice_sess_cand * cand = &cands[i];
                
                memset(cand, 0, sizeof(pj_ice_sess_cand));
                
                ret = -1;
                cand->comp_id = _json_get_int(json_val_cand, "component", -1);
                if (cand->comp_id <= 0) {
                    dbge("error comp_id %d", cand->comp_id);
                    break;
                }
                
                std::string founda = _json_get_string(json_val_cand, "foundation", std::string(""));
                //pj_cstr(&cand->foundation, founda.c_str());
                pj_strdup2_with_null(obj->ice_pool, &cand->foundation, founda.c_str());
                cand->prio = _json_get_int(json_val_cand, "priority", -1);
                
                std::string typ = _json_get_string(json_val_cand, "type",
                                                   std::string(""));
                if (typ == "host") {
                    cand->type = PJ_ICE_CAND_TYPE_HOST;
                } else if (typ == "srflx") {
                    cand->type = PJ_ICE_CAND_TYPE_SRFLX;
                } else if (typ == "relay") {
                    cand->type = PJ_ICE_CAND_TYPE_RELAYED;
                } else {
                    dbge("unknown candidate type: %s", typ.c_str());
                    break;
                }
                
                std::string ip = _json_get_string(json_val_cand, "ip",
                                                  std::string(""));
                int port = _json_get_int(json_val_cand, "port", 0);
                
#ifdef DEBUG_RELAY
                if(cand->type == PJ_ICE_CAND_TYPE_HOST){
                    port = 500 + i;
                }
#endif
                
                
                std::string rel_addr = _json_get_string(json_val_cand, "rel-addr", std::string(""));
                int rel_port = _json_get_int(json_val_cand, "rel-port", 0);
                
                // for ipv6
                make_addr(obj->ice_pool, ip.c_str(), port, obj->ice_cfg.af, &cand->addr);
                make_addr(obj->ice_pool, rel_addr.c_str(), rel_port, obj->ice_cfg.af, &cand->rel_addr);
                
                
                ret = 0;
                
                dump_cand(obj, cand, "remote cand");
            }
            if(ret !=0 ) break;
            cand_count = candidates.size();
        }
        
        
        eice_config * cfg = obj->cfg;
        if (!value["relayMS"].isNull() ){
            if (cfg->relay_ms.isNull()) {
                cfg->relay_ms = value["relayMS"];
                if (!cfg->relay_ms["caller"].isNull()) {
                    cfg->relay_caller = cfg->relay_ms["caller"];
                }
                
                if (!cfg->relay_ms["callee"].isNull()) {
                    cfg->relay_callee = cfg->relay_ms["callee"];
                }
            }else{
                // TODO: compare relay info here
            }
        }
        
        obj->remote_force_relay = _json_get_int(value, "forceRelay", 0);
        
        
        if(obj->role == PJ_ICE_SESS_ROLE_CONTROLLED){
            if(obj->remote_force_relay){
                cfg->turnAddrs.clear();
                cfg->turn_host = "";
                cfg->turn_port = -1;
                cfg->bind_cfg.stun_serv_num = 0;
            }
            else{
                if(value.isMember("turnAddrs")){
                    cfg->turnAddrs = _json_get_array(value, "turnAddrs",Json_em::Value());
                    if(!cfg->turnAddrs.isNull()){
                        dbgi("remote content contains recommend turn info");
                        ret = parse_turn_config(obj, cfg->turnAddrs, cfg->bind_cfg);
                        if(ret < 0){
                            dbge("parse remote turn fail!!!");
                            break;
                        }
                        
                        dbgi("remote turnAddrs num: %d", cfg->bind_cfg.stun_serv_num);
                        for(int i = 0; i < cfg->bind_cfg.stun_serv_num; i++){
                            dbgi("remote turnAddrs[%d]: %s:%d", i, cfg->bind_cfg.stun_serv_ips[i].ptr, cfg->bind_cfg.stun_serv_ports[i]);
                        }
                        
                        // always use the first turn as default
                        if(cfg->bind_cfg.stun_serv_num > 0){
                            cfg->turn_host = std::string(cfg->bind_cfg.stun_serv_ips[0].ptr, cfg->bind_cfg.stun_serv_ips[0].slen);
                            cfg->turn_port = cfg->bind_cfg.stun_serv_ports[0];
                        }
                        
                        ret = 0;
                    }
                }
            }
            
            
        }
        
    }while(0);
    return ret;
}


static int _parse_remote(eice_t obj, const char * remote_content, int remote_content_len){
    int ret = 0;
    do{
        dbgi("    remote_content %s", remote_content);
        
        if(obj->remote_cand_count == 0){
            // parse remote content
            std::string ufrag;
            std::string pwd;
            ret = parse_content(obj, remote_content, remote_content_len, ufrag,
                                pwd, obj->remote_cands, obj->remote_cand_count);
            if (ret != 0) {
                dbge("parse remote content fail !!!");
                break;
            }
            
            if(obj->remote_cand_count > 0){
                //		pj_cstr(&rufrag, ufrag.c_str());
                pj_strdup2_with_null(obj->ice_pool, &obj->rufrag, ufrag.c_str());
                
                //		pj_cstr(&rpwd, pwd.c_str());
                pj_strdup2_with_null(obj->ice_pool, &obj->rpwd, pwd.c_str());
                
                obj->str_remote_ufrag = new std::string(ufrag.c_str());
                obj->str_remote_pwd = new std::string(pwd.c_str());
            }
            
            
        }else{
            dbgi("already parsed remote content");
        }
    }while(0);
    return ret;
}



int eice_new(const char* config, int _role, const char * remote_content, int remote_content_len, eice_t * pobj) {

	int ret = -1;
	eice_t obj = 0;
	eice_config * cfg = 0;

	register_eice_thread(g_eice->pool);
	dbg("eice_new");
	
    pj_ice_sess_role role = (pj_ice_sess_role)_role;
    //close_steal_sockets(role);
    
	do {
		obj = (eice_t) malloc(sizeof(struct eice_st));
        memset(obj, 0, sizeof(struct eice_st));
		
        pj_ansi_snprintf(obj->obj_name, sizeof(obj->obj_name),
                         "eice%p", obj);
		
		obj->role = role;
				

		// create pool factory
		pj_caching_pool_init(&obj->cp, NULL, 0);
		obj->cp_inited = 1;

		/* Init our ICE settings with null values */
		pj_ice_strans_cfg_default(&obj->ice_cfg);

		// set caching pool factory
		obj->ice_cfg.stun_cfg.pf = &obj->cp.factory;

		/* Create application memory pool */
		obj->ice_pool = pj_pool_create(&obj->cp.factory, "eice_pool", 512, 512, NULL);
        		
		obj->init_steal_sockets(g_eice->pool);

        dbgi("config: %s", config);
        
        cfg = parse_eice_config(obj, config);
        if(!cfg){
            ret = -1;
            break;
        }
        dump_eice_config(obj, cfg);
        obj->cfg = cfg;
        
        obj->ice_cfg.af = pj_AF_INET();
        // for ipv6
        // must set env af (obj->ice_cfg.af) before _parse_remote
        if (!cfg->turn_host.empty()) {
            // for ipv6
            char ipv6[128];
            if(ipv4ToIpv6(cfg->turn_host.c_str(), ipv6) == 0){
                dbgi("turn_host ipv6: %s -> %s", cfg->turn_host.c_str(), ipv6);
                obj->ice_cfg.af = pj_AF_INET6();
                cfg->turn_host = ipv6;
                
            }

        }else{
            // for ipv6
            if(ipv4ToIpv6("121.41.105.183", NULL) == 0){
                obj->ice_cfg.af = pj_AF_INET6();
            }
        }
        
        dbgi("detected env af: %s", obj->ice_cfg.af == pj_AF_INET6() ? "ipv6" : "ipv4");
        

        
        
        
        // parse remote content here because of recommending turn by caller
        if(remote_content && remote_content_len > 0){
            ret = _parse_remote(obj, remote_content, remote_content_len);
            if(ret) break;
        }
        
        

		ret = pj_lock_create_recursive_mutex(obj->ice_pool, NULL, &obj->lock);
		if (ret != PJ_SUCCESS) {
			dbge("failed to create lock, ret=%d", ret);
			break;
		}
        
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
				&obj->ice_cfg.stun_cfg.timer_heap);
		if (ret != PJ_SUCCESS) {
			dbge("failed to create timer heap, ret=%d", ret);
			break;
		}

		/* and create ioqueue for network I/O stuff */
		ret = pj_ioqueue_create(obj->ice_pool, 16, &obj->ice_cfg.stun_cfg.ioqueue);
		if (ret != PJ_SUCCESS) {
			dbge("failed to create ioqueue, ret=%d", ret);
			break;
		}

		/* something must poll the timer heap and ioqueue,
		 * unless we're on Symbian where the timer heap and ioqueue run
		 * on themselves.
		 */
		ret = pj_thread_create(obj->ice_pool, "eice_thread",
				&eice_worker_thread, obj, 0, 0, &obj->ice_thread);
		if (ret != PJ_SUCCESS) {
			dbge("failed to create worker thread, ret=%d", ret);
			break;
		}

        
		// stun server
		if (!cfg->turn_host.empty()) {
            
//            // for ipv6
//            char ipv6[128];
//            if(ipv4ToIpv6(cfg->turn_host.c_str(), ipv6) == 0){
//                dbgi("turn_host ipv6: %s -> %s", cfg->turn_host.c_str(), ipv6);
//                obj->ice_cfg.af = pj_AF_INET6();
//                cfg->turn_host = ipv6;
//                
//            }
            
			/* Maximum number of host candidates */
			obj->ice_cfg.stun.max_host_cands = 1;

			/* Nomination strategy */
			obj->ice_cfg.opt.aggressive = PJ_TRUE;


			/* Configure STUN/srflx candidate resolution */
			pj_str_t srv = pj_str((char *)cfg->turn_host.c_str());
			pj_strassign(&obj->ice_cfg.stun.server, &srv);
			obj->ice_cfg.stun.port = (pj_uint16_t)  cfg->turn_port;

			/* stun keep alive interval */
			obj->ice_cfg.stun.cfg.ka_interval = KEEP_ALIVE_INTERVAL;
		} else {
			dbgi("no stun server info provided.");
            
//            // for ipv6
//            if(ipv4ToIpv6("121.41.105.183", NULL) == 0){
//                obj->ice_cfg.af = pj_AF_INET6();
//            }
		}

        // for ipv6
        // convert stun addr to ipv6, must after _parse_remote
        if(obj->ice_cfg.af == pj_AF_INET6()){
            for(int ui = 0; ui < cfg->bind_cfg.stun_serv_num; ui++){
                pj_str_t str = cfg->bind_cfg.stun_serv_ips[ui];
                char ipv6[128];
                if(ipv4ToIpv6(str.ptr, ipv6) == 0){
                    dbgi("turn_hosts ipv6: %s -> %s", str.ptr, ipv6);
                    pj_strdup2_with_null(obj->ice_pool, &cfg->bind_cfg.stun_serv_ips[ui], ipv6);
                }else{
                    dbgi("turn_hosts ipv6 fail: %s", str.ptr);
                }
                
            }
        }
        
        
        if(cfg->turn_host.empty() && obj->cfg->bind_cfg.stun_serv_num == 0){
            obj->force_relay = 1;
            dbgi("set force_relay");
        }
        
        
        dbgi("before ice: role=%s, force_relay=%d", pj_ice_sess_role_name(obj->role), obj->force_relay);
        
        
        dbgi("create ice strans...");
        // ice callback
        pj_ice_strans_cb icecb;
        pj_bzero(&icecb, sizeof(icecb));
        icecb.on_ice_complete = cb_on_ice_complete;
        
        // create ice strans instance;
        if(obj->cfg->bind_cfg.stun_serv_num == 0){
            ret = pj_ice_strans_create(NULL, &obj->ice_cfg,
                                       cfg->comp_count, obj, &icecb,
                                       &obj->icest);
        }else{
            obj->cfg->bind_cfg.stun_serv_num = 1; // always only use the first turn
            ret = pj_ice_strans_create_ext(NULL, &obj->ice_cfg,
                                           cfg->comp_count, obj, &icecb,
                                           &obj->cfg->bind_cfg,
                                           &obj->icest);
        }
        
        if (ret != PJ_SUCCESS) {
            pj_str_t es = pj_strerror(ret, obj->err_str, sizeof(obj->err_str));
            dbge("error creating ice strans, ret=%d(%s)", ret, es.ptr);
            break;
        }
        dbgi("create ice strans OK");
        

        
        

//		ret = query_wait(obj->lock, &obj->ice_init_done, NULL, 30*1000);
//		if (ret != 0) {
//			dbge("wait ice init timeout!!!");
//			break;
//		}
//
//		if(obj->ice_init_result != PJ_SUCCESS){
//			dbge("got ice init done, but result is fail!!!");
//			ret = obj->ice_init_result;
//			break;
//		}

//		ret = _get_local(obj);
//		if(ret != PJ_SUCCESS){
//			break;
//		}


		// get local =====>

//		// create ice session after ice strans inited;
//		ret = pj_ice_strans_init_ice(obj->icest, obj->role, 0, 0);
//        if (ret != PJ_SUCCESS) {
//            dbge("error init ice session, ret=%d", ret);
//            break;
//        }
//
//
////		dbgi("sleeping 2000 ms");
////		pj_thread_sleep(2000);
//
//		// get local ufrag and pwd
//
//		pj_str_t loc_ufrag;
//		pj_str_t loc_pwd;
//		ret = pj_ice_strans_get_ufrag_pwd(obj->icest, &loc_ufrag, &loc_pwd, NULL, NULL); // ignore remote.
//		if (ret != PJ_SUCCESS) {
//			dbge("error get ufrag and pwd, ret=%d", ret);
//			break;
//		}
//
////		strncpy(obj->local_ufrag, loc_ufrag.ptr, loc_ufrag.slen);
////		obj->local_ufrag[loc_ufrag.slen] = 0;
////		strncpy(obj->local_pwd, loc_pwd.ptr, loc_pwd.slen);
////		obj->local_pwd[loc_pwd.slen] = 0;
////		dbgi("local ufrag: %s", obj->local_ufrag);
////		dbgi("local pwd: %s", obj->local_pwd);
//
//
//		std::string str_local_ufrag(loc_ufrag.ptr, loc_ufrag.slen);
//		std::string str_local_pwd(loc_pwd.ptr, loc_pwd.slen);
//		dbgi("local ufrag: %s, len=%d", str_local_ufrag.c_str(), loc_ufrag.slen);
//		dbgi("local pwd: %s, len=%d", str_local_pwd.c_str(), loc_pwd.slen);
//
//
//
//		// get local candidates
//		Json_em::Value json_val_cands;
//		ret = PJ_SUCCESS;
//		for(int comp = 1; comp <= cfg->comp_count; comp++){
//			unsigned candCount = MAX_CANDIDATE_COUNT;
//			pj_ice_sess_cand cands[MAX_CANDIDATE_COUNT];
//			memset(cands, 0, sizeof(pj_ice_sess_cand) * MAX_CANDIDATE_COUNT);
//
//			ret = pj_ice_strans_enum_cands(obj->icest, comp, &candCount, cands);
//			if (ret != PJ_SUCCESS) {
//				dbge("error get candidates of comp %d", comp);
//				break;
//			}
//			dbgi("comp %d has candidates %d", comp, candCount);
//
//			for(int ci = 0; ci < candCount; ci++){
//				pj_ice_sess_cand * cand = &cands[ci];
//
//				// make candidate JSON
//				Json_em::Value json_val_cand;
//				cand_to_json_value(comp, ci, cand, json_val_cand);
//
//				json_val_cands.append(json_val_cand);
//
//			}
//		}
//		if (ret != PJ_SUCCESS) break;
//
//		Json_em::Value json_val;
//		json_val.clear();
//		json_val["ufrag"] = str_local_ufrag;
//		json_val["pwd"] = str_local_pwd;
//		json_val["candidates"] = json_val_cands;
//
//		Json_em::FastWriter writer;
//		obj->local_content = new std::string(writer.write(json_val));
//
//		dbgi("local-content= %s\n", obj->local_content->c_str());
//
//
////		std::string sytled_str = json_val.toStyledString();
////		dbgi("styled-local-content=  %s\n", sytled_str.c_str());


		// get local <=====

		*pobj = obj;
		ret = 0;

	} while (0);

	if(ret != 0){
		eice_free(obj);
	}

	return ret;
}


int eice_get_local(eice_t obj, char * local_content, int * p_local_content_len){
	int ret = 0;
	register_eice_thread(g_eice->pool);
	dbgi("eice_get_local");
	pj_lock_acquire(obj->lock);
	if(!obj->local_content)
	{
        ret = _get_local(obj);
//		if(obj->ice_init_result == PJ_SUCCESS){
//			ret = _get_local(obj);
//		}else{
//			ret = obj->ice_init_result;
//		}

	}

	if(obj->local_content){
		strcpy(local_content, obj->local_content->c_str());
		*p_local_content_len = (int) obj->local_content->size();
	}

	pj_lock_release(obj->lock);
	return ret;
}

void eice_free(eice_t obj){
	if(!obj) return ;

    register_eice_thread(g_eice->pool);

    dbgi("eice_free of %llu", (uint64_t)obj);

    dbgi("eice_free: stopping ioq");
    obj->ioq_stop_req = 1;
    if (obj->ice_thread) {
    	while(!obj->ioq_stopped){
    		pj_thread_sleep(300);
    	}
    }

    dbgi("eice_free: stealing socket");
    //update_steal_sockets(obj);
    
    if(obj->grp_lock){
        pj_timer_heap_cancel_if_active(obj->ice_cfg.stun_cfg.timer_heap, &obj->timer, 0);
    }
    
    
    dbgi("eice_free: delete confice");
    if(obj->confice){
        confice_free(obj->confice);
        obj->confice = 0;
    }
    

    dbgi("eice_free: delete ice trans");
    // stop strans
    if (obj->icest) {

    	// stop session
		if (pj_ice_strans_has_sess(obj->icest)) {
			dbgi("eice_free: stop ice");
			pj_ice_strans_stop_ice(obj->icest);
		}

		dbgi("eice_free: destroy ice trans");
        pj_ice_strans_destroy(obj->icest);
        obj->icest = 0;
    }


    // stop thread
    dbgi("eice_free: stopping thread");
    obj->thread_quit_flag = 1;
    if (obj->ice_thread) {
    	dbgi("eice_free: waiting for thread");
    	while(!obj->thread_exited_flag){
    		pj_thread_sleep(300);
    	}
    	dbgi("eice_free: thread finish");
        pj_thread_join(obj->ice_thread);
        pj_thread_destroy(obj->ice_thread);
        obj->ice_thread = 0;
    }


    // stop io queue
    if (obj->ice_cfg.stun_cfg.ioqueue) {
        pj_ioqueue_destroy(obj->ice_cfg.stun_cfg.ioqueue);
        obj->ice_cfg.stun_cfg.ioqueue = 0;
    }


    // stop timer
    if (obj->ice_cfg.stun_cfg.timer_heap) {
        pj_timer_heap_destroy(obj->ice_cfg.stun_cfg.timer_heap);
        obj->ice_cfg.stun_cfg.timer_heap = 0;
    }


	if(obj->local_content){
		delete obj->local_content;
		obj->local_content = 0;
	}

	if (obj->remote_content) {
		delete obj->remote_content;
		obj->remote_content = 0;
	}


    if(obj->str_local_ufrag){
        delete obj->str_local_ufrag;
        obj->str_local_ufrag = 0;
    }

    if(obj->str_local_pwd){
        delete obj->str_local_pwd;
        obj->str_local_pwd = 0;
    }

    if(obj->str_remote_ufrag){
        delete obj->str_remote_ufrag;
        obj->str_remote_ufrag = 0;
    }

    if(obj->str_remote_pwd){
        delete obj->str_remote_pwd;
        obj->str_remote_pwd = 0;
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
            dbge("eice: release lock return %d, NOT PJ_EGONE !!!", s);
        }
        obj->grp_lock = 0;
    }
    
    
    
    if (obj->lock) {
		pj_lock_destroy(obj->lock);
		obj->lock = 0;
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


int eice_new_caller(const char* config, char * local_content,
		int * p_local_content_len, eice_t * pobj) {
	int ret = -1;
	eice_t obj = 0;
    
    register_eice_thread(g_eice->pool);
    dbg("eice_new_caller");
    
	if (strlen(config) == 0)
		return ret;

	do{
		ret = eice_new(config, PJ_ICE_SESS_ROLE_CONTROLLING, NULL, 0, &obj);
		if(ret){
			break;
		}

        if(obj->icest){
            ret = query_wait(obj, obj->lock, &obj->ice_init_done, NULL, 30*1000);
            if (ret != 0) {
                dbge("wait ice init timeout!!!");
                break;
            }
        }
		

		ret = eice_get_local(obj, local_content, p_local_content_len);
		if (ret != PJ_SUCCESS) {
			break;
		}


//        dbgi("obj->local_content = %p", obj->local_content);
//        dbgi("obj->local_content->size() = %d", obj->local_content->size());
//        dbgi("local_content = %p", local_content);
//        dbgi("p_local_content_len = %p", p_local_content_len);
        

//		strcpy(local_content, obj->local_content->c_str());
//		*p_local_content_len = obj->local_content->size();

		*pobj = obj;
	}while(0);

	if(ret != 0){
		eice_free(obj);
	}
	return ret;
}




int eice_start_nego(eice_t obj, const char * remote_content, int remote_content_len){
	int ret = -1;
    dbgi("eice_start_nego ---------->");
	do {

		ret = _parse_remote(obj, remote_content, remote_content_len);
		if(ret) break;

//        dbgi("    remote_content %s", remote_content);
//
//        if(obj->remote_cand_count == 0){
//			// parse remote content
//			std::string ufrag;
//			std::string pwd;
//			ret = parse_content(obj, remote_content, remote_content_len, ufrag,
//					pwd, obj->remote_cands, obj->remote_cand_count);
//			if (ret != 0) {
//				dbge("parse remote content fail !!!");
//				break;
//			}
//			pj_str_t rufrag, rpwd;
//			//		pj_cstr(&rufrag, ufrag.c_str());
//			pj_strdup2_with_null(obj->ice_pool, &rufrag, ufrag.c_str());
//
//			//		pj_cstr(&rpwd, pwd.c_str());
//			pj_strdup2_with_null(obj->ice_pool, &rpwd, pwd.c_str());
//        }else{
//        	dbgi("already parsed remote content");
//        }

        dbgi("creating confice...");
        confice_cb cb;
        pj_bzero(&cb, sizeof(confice_cb));
        cb.on_complete = &on_confice_complete;
		if (obj->role == PJ_ICE_SESS_ROLE_CONTROLLING && !obj->cfg->relay_caller.isNull()) {
			ret = confice_new(obj->cfg->relay_caller, &obj->ice_cfg, obj->ice_pool,
                    obj->ice_cfg.af,
                    obj->role,
                    *obj->str_local_ufrag,
                    *obj->str_local_pwd,
                    *obj->str_remote_ufrag,
                    *obj->str_remote_pwd,
                    &cb,
                    obj,
                    &obj->confice);
		} else if (obj->role == PJ_ICE_SESS_ROLE_CONTROLLED && !obj->cfg->relay_callee.isNull()){
			ret = confice_new(obj->cfg->relay_callee, &obj->ice_cfg, obj->ice_pool,
                    obj->ice_cfg.af,
                    obj->role,
                    *obj->str_local_ufrag,
                    *obj->str_local_pwd,
                    *obj->str_remote_ufrag,
                    *obj->str_remote_pwd,
                    &cb,
                    obj,
                    &obj->confice);
		}
        if(ret){
            dbge("creating confice fail!!!");
            break;
        }
        
        if(obj->confice){
            dbgi("creating confice ok");
        }else{
            dbgi("skip creating confice");
        }
        

        dbgi("    icest = 0x%p", obj->icest);
        dbgi("    remote_cand_count %d", obj->remote_cand_count);
        dbgi("    remote_force_relay %d", obj->remote_force_relay);
        
        if(!obj->remote_force_relay && obj->force_relay){
            dbgi("remote NOT support force relay");

        }
        
        if(obj->icest && !obj->force_relay){
            ret = pj_ice_strans_start_ice(obj->icest, &obj->rufrag, &obj->rpwd,
                                          obj->remote_cand_count, obj->remote_cands);
            if (ret != 0) {
                dbge("start ice fail !!!");
                break;
            }
            dbgi("start ice OK");
            
            pj_time_val delay;
            delay.sec = ICE_TIMEOUT_MS/1000;
            delay.msec = ICE_TIMEOUT_MS%1000;
            
            pj_timer_heap_schedule_w_grp_lock(obj->ice_cfg.stun_cfg.timer_heap,
                                              &obj->timer, &delay, PJ_TRUE, obj->grp_lock);
            
        }else{
            dbgi("skip start ice");
        }
		
        
        
        if(obj->confice){
            confice_kickoff_reg(obj->confice);
        }
        if(obj->confice && obj->role == PJ_ICE_SESS_ROLE_CONTROLLING){
            if(!obj->icest || obj->remote_force_relay || obj->force_relay){
                dbgi("kick confice seleting directly");
                confice_kickoff_select_relay(obj->confice);
            }
        }
        
        
	} while (0);
    dbgi("eice_start_nego <----------");
	return ret;
}

int eice_new_callee(const char* config, const char * remote_content, int remote_content_len,
		char * local_content, int * p_local_content_len,
		eice_t * pobj){
	int ret = -1;
	eice_t obj = 0;
    
    register_eice_thread(g_eice->pool);
    dbg("eice_new_callee");
	do {
		ret = eice_new(config, PJ_ICE_SESS_ROLE_CONTROLLED, remote_content, remote_content_len, &obj);
		if (ret) {
			break;
		}

        if(obj->icest){
            ret = query_wait(obj, obj->lock, &obj->ice_init_done, NULL, 30 * 1000);
            if (ret != 0) {
                dbge("wait ice init timeout!!!");
                break;
            }
        }
		

//		ret = _parse_remote(obj, remote_content, remote_content_len);
//				if(ret) break;

		ret = eice_get_local(obj, local_content, p_local_content_len);
		if (ret != PJ_SUCCESS) {
			break;
		}


		ret = eice_start_nego(obj, remote_content, remote_content_len);
		if(ret != 0){
			dbge("callee start nego fail !!!");
			break;
		}
		dbgi("callee start nego OK");

//		strcpy(local_content, obj->local_content->c_str());
//		*p_local_content_len = obj->local_content->size();

		*pobj = obj;
	} while (0);

	if (ret != 0) {
		eice_free(obj);
	}
	return ret;
}

int eice_caller_nego(eice_t obj, const char * remote_content, int remote_content_len,
		eice_on_nego_result_t cb, void * cbContext )
{
	int ret = -1;
    
    register_eice_thread(g_eice->pool);
    
	do{
		ret = eice_start_nego(obj, remote_content, remote_content_len);
		if (ret != 0) {
			dbge("caller start nego fail !!!");
			break;
		}
		dbgi("caller start nego OK");
		ret = 0;
	} while (0);
	return ret;
}

int eice_steal_fds(eice_t obj, pj_sock_t fds[], int * pnum){
    *pnum = 0;
    
	int nego_done = 1; // default done, in case of icest disabled
	pj_status_t nego_status = -1;

    int confice_done = 1;  // default done, in case of confice disabled
    pj_status_t confice_status = -1;

    register_eice_thread(g_eice->pool);

	pj_lock_acquire(obj->lock);

    if(obj->icest && !obj->force_relay){
        nego_done = obj->ice_nego_done;
        nego_status = obj->ice_nego_status;
    }

    if(obj->confice){
        confice_done = obj->confice_done;
        confice_status = obj->confice_status;
    }

	pj_lock_release(obj->lock);

	if(!nego_done) return -1;
    if(!confice_done) return -1;

    int sz = *pnum;
    int fd_num = 0;
    if (nego_status == PJ_SUCCESS) {
    	for (int comp_id = 1; comp_id <= obj->cfg->comp_count; comp_id++) {
    		fds[fd_num] = pj_ice_strans_steal_comp_stun_fd(obj->icest, comp_id);
    		fd_num++;
    	}
        *pnum = fd_num;
    	return 0;
    }else if(obj->confice){
    	int num = sz - fd_num;
    	int ret = confice_steal_fds(obj->confice, &fds[fd_num], &num);
    	if(ret == 0){
    		fd_num += num;
    	}
        *pnum = fd_num;
    	return 0;
    }
    return -1;
}

int eice_get_nego_result(eice_t obj, char * nego_result, int * p_nego_result_len){
	int nego_done = 1; // default done, in case of icest disabled
	pj_status_t nego_status = -1;
    
    int confice_done = 1;  // default done, in case of confice disabled
    pj_status_t confice_status = -1;
    
    register_eice_thread(g_eice->pool);
    
	pj_lock_acquire(obj->lock);
    
    if(obj->icest && !obj->force_relay){
        nego_done = obj->ice_nego_done;
        nego_status = obj->ice_nego_status;
    }
    
    if(obj->confice){
        confice_done = obj->confice_done;
        confice_status = obj->confice_status;
    }
    
	pj_lock_release(obj->lock);

	if(!nego_done) return -1;
    if(!confice_done) return -1;
    
    dbgi("eice_get_nego_result: nego_status %d, confice_status %d", nego_status, confice_status);
    
#ifdef DEBUG_RELAY
    nego_status = -1; // for debug relay
#endif
    
	Json_em::Value root_val;
	Json_em::Value pairs_val;
	
	if (nego_status == PJ_SUCCESS) {
		root_val["result"] = 0;

		for (int comp_id = 1; comp_id <= obj->cfg->comp_count; comp_id++) {
			const pj_ice_sess_check * sess_check = pj_ice_strans_get_valid_pair(
					obj->icest, comp_id);
			if ((sess_check != NULL) && (sess_check->nominated == PJ_TRUE)) {

				Json_em::Value local_val;
				local_cand_to_json_value(comp_id, 0, sess_check->lcand, local_val);

				Json_em::Value remote_val;
				cand_to_json_value(comp_id, 0, sess_check->rcand, remote_val);

				Json_em::Value pair_val;
				pair_val["comp_id"] = comp_id;
				pair_val["local"] = local_val;
				pair_val["remote"] = remote_val;
				pairs_val.append(pair_val);
			}
		}
		root_val["pairs"] = pairs_val;

	} else if(obj->confice && confice_get_result(obj->confice, &pairs_val) == 0){
		root_val["result"] = 0;
		root_val["relay_pairs"] = pairs_val;
	}
	else {
		root_val["result"] = -1;
	}

	Json_em::FastWriter writer;
	std::string str = writer.write(root_val);
	dbgi("============= nego result == %s @@@\n", root_val.toStyledString().c_str());

	strcpy(nego_result, str.c_str());
	*p_nego_result_len = (int)str.size();

	return 0;
}

static pj_status_t handle_events(eice_t obj, unsigned max_msec, unsigned *p_count) {

//    register_eice_thread(g_eice->pool);

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
    c = pj_timer_heap_poll( obj->ice_cfg.stun_cfg.timer_heap, &timeout );
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

	if(obj->ioq_stop_req){
		if(!obj->ioq_stopped){
			obj->ioq_stopped = 1;
			dbgi("stop ioq polling");
		}
		pj_thread_sleep((unsigned int)PJ_TIME_VAL_MSEC(timeout));
		return 0;
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
        c = pj_ioqueue_poll( obj->ice_cfg.stun_cfg.ioqueue, &timeout);
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

static int eice_worker_thread(void *arg) {

	eice_t obj = (eice_t) arg;

//    register_eice_thread(g_eice->pool);
	dbgi("eice_worker_thread start %p ==> ", obj);

    while (!obj->thread_quit_flag) {
        handle_events(obj, 500, NULL);
    }
    
    dbgi("eice_worker_thread exited %p <== ", obj);
    obj->thread_exited_flag = 1;

    return 0;
}

/// add by rao
void eice_set_get_nego_result_func(nego_result_func result_func)
{
	assert(result_func != NULL);
	g_nego_result_func = result_func;
}

void eice_set_log_func(eice_log_func * log_func){
    pj_log_set_log_func(log_func);
}



void eice_st::init_steal_sockets(pj_pool_t * pool){
	for (int i = 0; i < MAX_STEAL_SOCKET; i++){
		g_steal_sockets[i] = -1;
	}

//	pj_status_t ret = pj_grp_lock_create(pool, NULL, &g_steal_lock);
//	if (ret != PJ_SUCCESS) {
//		dbg("init_steal_sockets: create lock fail");
//		return;
//	}
	//pj_grp_lock_add_ref(g_steal_lock);
}

void eice_st::close_steal_sockets(){
	//pj_grp_lock_acquire(g_steal_lock);

	for (int i = get_streal_start(role); i < get_streal_end(role); i++){
		if (g_steal_sockets[i] > 0){
			pj_sock_close(g_steal_sockets[i]);
			g_steal_sockets[i] = -1;
			dbg("close_steal_sockets: [%d] -> %d", i, g_steal_sockets[i]);
		}
	}
	//	g_steal_socket_count = 0;

	//pj_grp_lock_release(g_steal_lock);
}

void eice_st::update_steal_sockets(){
	//close_steal_sockets();

	//pj_grp_lock_acquire(g_steal_lock);

	int start = get_streal_start(this->role);
	int num = get_streal_end(this->role) - start;
	int ret = eice_steal_fds(this, g_steal_sockets + start, &num);
	if (ret == 0){
		for (int i = 0; i < num; i++){
			int index = start + i;
			dbg("update_steal_sockets: [%d] -> %d", index, g_steal_sockets[index]);
		}

		if ((start + num) > g_steal_socket_count){
			dbg("update_steal_sockets: g_steal_socket_count %d -> %d", g_steal_socket_count, (start + num));
			g_steal_socket_count = (start + num);
		}
	}

	//pj_grp_lock_release(g_steal_lock);
}

int eice_st::eice_get_global_socket(int port){

	if (!g_eice){
		//		dbg("eice_get_global_socket:  no eice obj, port %d ", port);
		printf("eice_get_global_socket:  no eice obj, port %d ", port);
		return -1;
	}

	register_eice_thread(g_eice->pool);

	dbg("eice_get_global_socket: try  get port %d ", port);

	if (port <= 0){
		dbg("eice_get_global_socket:  port %d < 0 ", port);
		return -1;
	}


	//pj_grp_lock_acquire(g_steal_lock);

	int ret_fd = -1;
	for (int i = 0; i < g_steal_socket_count; i++){
		pj_sock_t fd = (int)g_steal_sockets[i];

		if (fd > 0){
			//		    struct sockaddr_in local_addr;
			//		    int local_sz = sizeof(local_addr);
			//		    if (0 == getsockname(fd, (struct sockaddr *)&local_addr, &local_sz))
			//		    {
			//		    }
			//		    int local_port = ntohs(local_addr.sin_port);
			//		    if(local_port == port){
			//		    	g_steal_sockets[i] = -1;
			//		    	return fd;
			//		    }

			pj_sockaddr_in local_addr;
			int addr_len = sizeof(pj_sockaddr_in);
			int status = pj_sock_getsockname(fd, &local_addr, &addr_len);
			if (status == PJ_SUCCESS) {
				int local_port = pj_sockaddr_get_port(&local_addr);
				if (local_port == port){
					g_steal_sockets[i] = -1;
					dbg("eice_get_global_socket: [%d] %d -> %d", i, fd, g_steal_sockets[i]);
					ret_fd = (int)fd;
					break;
				}
			}
		}

	}
	if (ret_fd < 0){
		dbg("eice_get_global_socket:  no match fd, port %d ", port);
	}
	//pj_grp_lock_release(g_steal_lock);
	return ret_fd;
}
int eice_get_global_socket(eice_t obj, int port) {
	return obj->eice_get_global_socket(port);
}
void update_steal_sockets(eice_t obj) {
	obj->update_steal_sockets();
}

//#include <netinet/in.h>
//#include <sys/socket.h>
//static void addr2str(const struct sockaddr_in * addr, char * str){
//    const char *ip = inet_ntoa(addr->sin_addr);
//    int port = ntohs(addr->sin_port);
//    sprintf(str, "%s:%d", ip, port);
//
//}




void test_log_func(int level, const char *data, int len){
//    printf("%s", data);
}


static int _test_with_manual(){
    //    new InetSocketAddress("203.195.185.236", 3488), // turn1
    //    new InetSocketAddress("121.41.75.10", 3488), // turn3
    
    //    const char * config_json = "{\"turnHost\":\"203.195.185.236\",\"turnPort\":3488,\"compCount\":2}";
    //    const char * config_json = "{\"turnHost\":\"203.195.185.36\",\"turnPort\":3488,\"compCount\":2}"; // error ip
    
    
    const char * caller_config_json = "{"
    //        "\"turnHost\":\"203.195.185.236\",\"turnPort\":3488,"
    "\"compCount\":2"
    ",\"turnAddrs\":["
    "{\"host\":\"203.195.185.236\",\"port\":3488}," // turn1
    "{\"host\":\"121.41.75.10\",\"port\":3488}" // turn3
    "]"
    
    //            ","
    //    	"\"relayMS\":{"
    //        	"\"caller\":{"
    //    			"\"conferenceId\" : \"video2p-6ed76c6c-6115-42de-8777-fddd200ba6aa\","
    //    			"\"serverIp\" : \"172.16.3.92\","
    //    			"\"rcode\" : \"1449664953\","
    //    			"\"serverPort\" : 5000,"
    //    			"\"channelId\" : 1,"
    //        		"\"vchannelId\" : 2"
    //        	"},"
    //
    //    		"\"callee\":{"
    //    			"\"conferenceId\" : \"video2p-6ed76c6c-6115-42de-8777-fddd200ba6aa\","
    //    			"\"serverIp\" : \"172.16.3.92\","
    //    			"\"rcode\" : \"1013325739\","
    //    			"\"serverPort\" : 5000,"
    //    			"\"channelId\" : 3,"
    //        		"\"vchannelId\" : 4"
    //    		"}"
    //    	"}"
    
    "}";
    
    const char * callee_config_json = "{"
    "\"compCount\":2"
    ",\"turnAddrs\":["
    "{\"host\":\"203.195.185.236\",\"port\":348},"// turn1
    "{\"host\":\"121.41.75.10\",\"port\":348}"// turn3
    "]"
    "}";
    
    //    const char * caller_config_json = "{\"compCount\":2,\"turnAddrs\":[{\"turnHost\":\"203.195.185.236\",\"turnPort\":3488}]}";
    //    const char * callee_config_json = caller_config_json;
    
    //"u1" : {
    //      "conferenceId" : "voicep2p-8c4eeb0d-0a45-4fe5-a3c2-5ab146e6dba2",
    //      "serverIp" : "10.0.1.161",
    //      "rcode" : "2027467697",
    //      "serverPort" : 5000,
    //      "channelId" : 1
    //    },
    
    
    int ret = 0;
    
    
    
    //    eice_set_log_func(test_log_func);
    
    char * caller_content = new char[8*1024];
    int caller_content_len = 0;
    eice_t caller = 0;
    
    
    char * callee_content = new char[8*1024];
    int callee_content_len = 0;
    eice_t callee = 0;
    
    char * caller_result = new char[8*1024];
    int caller_result_len = 0;
    char * callee_result = new char[8*1024];
    int callee_result_len = 0;
    
    do{
        ret = eice_new_caller(caller_config_json, caller_content, &caller_content_len, &caller);
        dbg("eice_new_caller return %d, caller=%p", ret, caller);
        if(ret){
            dbg("something wrong with new caller !!!");
            break;
        }
        
        ret = eice_new_callee(callee_config_json, caller_content, caller_content_len, callee_content, &callee_content_len, &callee);
        dbg("eice_new_callee return %d", ret);
        if(ret){
            dbg("something wrong with new callee !!!");
            break;
        }
        
        ret = eice_caller_nego(caller, callee_content, callee_content_len, 0, 0);
        dbg("eice_caller_nego return %d", ret);
        if(ret){
            dbg("something wrong with new caller nego !!!");
            break;
        }
        
        
        
        while(caller_result_len == 0 || callee_result_len == 0){
            if(caller_result_len == 0){
                ret = eice_get_nego_result(caller, caller_result, &caller_result_len);
                if(ret == 0 && caller_result_len > 0){
                    dbg("get caller nego result OK");
                }
            }
            if(callee_result_len == 0){
                ret = eice_get_nego_result(callee, callee_result, &callee_result_len);
                if(ret == 0 && callee_result_len > 0){
                    dbg("get callee nego result OK");
                }
            }
            pj_thread_sleep(50);
        }
        
        
        
    }while(0);
    
    
    
    dbg("test free caller n callee");
    if(caller){
        eice_free(caller);
        caller = 0;
    }
    if(callee){
        eice_free(callee);
        callee = 0;
    }
    
    
    
    //ret = eice_new_caller(config_json, caller_content, &caller_content_len, &caller);
    
    
    
    
    delete []caller_result;
    delete []caller_content;
    
    delete []callee_result;
    delete []callee_content;
    
    dbg("test case result: %s ==> %d", __FUNCTION__, ret);
    
    return ret;
}


class test_expect{
public:
    
    int result;
    pj_bool_t is_relay;
    std::string local_cand_type;
    std::string remote_cand_type;
    
    
    pj_bool_t local_is_host_cand;
    pj_bool_t local_is_srflx_cand;
    pj_bool_t local_is_ms_cand;
    pj_bool_t local_is_turn_addrs;
    
    test_expect(){
        this->result = 0;
        this->is_relay = PJ_FALSE;
        
        this->local_is_host_cand = PJ_TRUE;
        this->local_is_srflx_cand = PJ_TRUE;
        this->local_is_ms_cand = PJ_FALSE;
        this->local_is_turn_addrs = PJ_TRUE;
    }
    
    int check_content(const char * content){
        Json_em::Reader reader;
        Json_em::Value value;
        int ret = -1;
        do{
            if (!reader.parse(content, value)) {
                dbg("parse content fail!!!");
                ret = -1;
                break;
            }
            
            Json_em::Value cands_value = _json_get_array(value, "candidates", Json_em::Value());
            if(this->local_is_host_cand || this->local_is_srflx_cand){
                if(cands_value.isNull()){
                    dbg("empty candidates !!!");
                    ret = -1;
                    break;
                }
                
                ret = 0;
                pj_bool_t is_host_cand = PJ_FALSE;
                pj_bool_t is_srflx_cand = PJ_FALSE;
                for(unsigned int ui = 0; ui < cands_value.size(); ui++){
                    Json_em::Value cand_value = cands_value[ui];
                    std::string type = _json_get_string(cand_value, "type", "");
                    if(type.empty()){
                        dbg("NOT found type in candidate");
                        ret = -1;
                        break;
                    }
                    if(type == "host"){
                        is_host_cand = PJ_TRUE;
                    }
                    if(type == "srflx"){
                        is_srflx_cand = PJ_TRUE;
                    }
                }
                if(ret) break;
                
                if((this->local_is_host_cand && !is_host_cand)
                   || (!this->local_is_host_cand && is_host_cand)){
                    dbg("is_host_cand: expect %d but %d", this->local_is_host_cand, is_host_cand);
                    ret = -1;
                    break;
                    
                }
                
                if((this->local_is_srflx_cand && !is_srflx_cand)
                   || (!this->local_is_srflx_cand && is_srflx_cand)){
                    dbg("is_srflx_cand: expect %d but %d", this->local_is_srflx_cand, is_srflx_cand);
                    ret = -1;
                    break;
                    
                }
                
            }else{
                if(!cands_value.isNull()){
                    dbg("candidates: expect empty  !!!");
                    ret = -1;
                    break;
                }
            }
            

            pj_bool_t is_ms_cand = PJ_FALSE;
            Json_em::Value ms_value = _json_get_obj(value, "relayMS", Json_em::Value());
            if(!ms_value.isNull()){
                Json_em::Value caller_value = _json_get_obj(ms_value, "caller", Json_em::Value());
                Json_em::Value callee_value = _json_get_obj(ms_value, "callee", Json_em::Value());
                if(!caller_value.isNull() || !callee_value.isNull()){
                    is_ms_cand = PJ_TRUE;
                }
            }
            
            if((this->local_is_ms_cand && !is_ms_cand)
               || (!this->local_is_ms_cand && is_ms_cand)){
                dbg("is_ms_cand: expect %d but %d!!!", this->local_is_ms_cand, is_ms_cand);
                ret = -1;
                break;
            }
            
            
            
            pj_bool_t is_turn_addrs = PJ_FALSE;
            Json_em::Value turn_value = _json_get_array(value, "turnAddrs", Json_em::Value());
            if(!turn_value.isNull() && turn_value.size() > 0){
                is_turn_addrs = PJ_TRUE;
            }
            
            if((this->local_is_turn_addrs && !is_turn_addrs)
               || (!this->local_is_turn_addrs && is_turn_addrs)){
                dbg("is_turn_addrs: expect %d but %d!!!", this->local_is_turn_addrs, is_turn_addrs);
                ret = -1;
                break;
            }
            
            
            ret = 0;
        }while(0);
        
        return ret;
    }
    
    int check_result(const char * result_content){
        Json_em::Reader reader;
        Json_em::Value value;
        int ret = -1;
        do{
            if (!reader.parse(result_content, value)) {
                dbg("parse result fail!!!");
                ret = -1;
                break;
            }
            
            int result = _json_get_int(value, "result", -1);
            if(result != this->result){
                dbg("result: expect %d, but %d", this->result, result);
                ret = -1;
                break;
            }
            
            if(this->result != 0){
                ret = 0;
                break;
            }
            
            ret = -1;
            if(!this->is_relay){
                Json_em::Value pairs_value = _json_get_obj(value, "pairs", Json_em::Value());
                if(pairs_value.isNull()){
                    dbg("empty pairs !!!");
                    break;
                }
            }else{
                Json_em::Value relay_pairs_value = _json_get_obj(value, "relay_pairs", Json_em::Value());
                if(relay_pairs_value.isNull()){
                    dbg("empty relay pairs !!!");
                    break;
                }
            }
            
            ret = 0;
        }while(0);
        
        return ret;
    }
};
    
static int _test_with_config(const char * caller_config_json, const char * callee_config_json
                             , test_expect& caller_expect
                             , test_expect& callee_expect){

    int ret = 0;
    
    //    eice_set_log_func(test_log_func);
    
    char * caller_content = new char[8*1024];
    int caller_content_len = 0;
    eice_t caller = 0;
    
    
    char * callee_content = new char[8*1024];
    int callee_content_len = 0;
    eice_t callee = 0;
    
    char * caller_result = new char[8*1024];
    int caller_result_len = 0;
    char * callee_result = new char[8*1024];
    int callee_result_len = 0;
    
    do{
        ret = eice_new_caller(caller_config_json, caller_content, &caller_content_len, &caller);
        dbg("eice_new_caller return %d, caller=%p, content:%s.", ret, caller, caller_content);
        if(ret){
            dbg("something wrong with new caller !!!");
            break;
        }
        
        ret = caller_expect.check_content(caller_content);
        if(ret){
            dbg("check caller content fail !!!");
            break;
        }
        
        ret = eice_new_callee(callee_config_json, caller_content, caller_content_len, callee_content, &callee_content_len, &callee);
        dbg("eice_new_callee return %d", ret);
        if(ret){
            dbg("something wrong with new callee !!!");
            break;
        }
        
        ret = callee_expect.check_content(callee_content);
        if(ret){
            dbg("check callee content fail !!!");
            break;
        }
        
        
        
        ret = eice_caller_nego(caller, callee_content, callee_content_len, 0, 0);
        dbg("eice_caller_nego return %d", ret);
        if(ret){
            dbg("something wrong with new caller nego !!!");
            break;
        }
        
        
        
        while(caller_result_len == 0 || callee_result_len == 0){
            if(caller_result_len == 0){
                ret = eice_get_nego_result(caller, caller_result, &caller_result_len);
                if(ret == 0 && caller_result_len > 0){
                    dbg("get caller nego result OK");
                }
            }
            if(callee_result_len == 0){
                ret = eice_get_nego_result(callee, callee_result, &callee_result_len);
                if(ret == 0 && callee_result_len > 0){
                    dbg("get callee nego result OK");
                }
            }
            pj_thread_sleep(50);
        }
        
        eice_free(caller);
        caller = 0;
        
        eice_free(callee);
        callee = 0;
        
        ret = caller_expect.check_result(caller_result);
        if(ret != 0){
            dbg("caller expect fail !!!");
            break;
        }
        
        ret = callee_expect.check_result(callee_result);
        if(ret != 0){
            dbg("callee expect fail !!!");
            break;
        }
        
        ret = 0;
        
    }while(0);
    
    
    
    dbg("test done");
    if(caller){
        eice_free(caller);
        caller = 0;
    }
    if(callee){
        eice_free(callee);
        callee = 0;
    }
    
    
    
    //ret = eice_new_caller(config_json, caller_content, &caller_content_len, &caller);
    
    
    
    delete []caller_result;
    delete []caller_content;
    
    delete []callee_result;
    delete []callee_content;
    
    dbg("test case result: %s ==> %d", __FUNCTION__, ret);
    
    return ret;
}


static int _test_caller(const char * config_json
                             , test_expect& expect){
    
    int ret = 0;
    
    char * local_content = new char[8*1024];
    int local_content_len = 0;
    eice_t caller = 0;
    
    
    do{
        ret = eice_new_caller(config_json, local_content, &local_content_len, &caller);
        dbg("eice_new_caller return %d, caller=%p", ret, caller);
        if(ret){
            dbg("something wrong with new caller !!!");
            break;
        }
        
        eice_free(caller);
        caller = 0;
        
        ret = expect.check_content(local_content);
        if(ret){
            dbg("check caller content fail !!!");
            break;
        }
        
        ret = 0;
        
    }while(0);
    
    
    if(caller){
        eice_free(caller);
        caller = 0;
    }
    
    delete []local_content;
    
    
    dbg("test case result: %s ==> %d", __FUNCTION__, ret);
    
    return ret;
}


static int _test_callee(const char * config_json
                        ,const char * remote_content, int remote_content_len
                        , test_expect& expect){
    
    int ret = 0;
    
    char * local_content = new char[8*1024];
    int local_content_len = 0;
    eice_t call = 0;
    
    
    do{
        ret = eice_new_callee(config_json, remote_content, remote_content_len, local_content, &local_content_len, &call);
        dbg("eice_new_callee return %d, call=%p", ret, call);
        if(ret){
            dbg("something wrong with new callee !!!");
            break;
        }
        
        eice_free(call);
        call = 0;
        
        ret = expect.check_content(local_content);
        if(ret){
            dbg("check local content fail !!!");
            break;
        }
        
        ret = 0;
        
    }while(0);
    
    
    if(call){
        eice_free(call);
        call = 0;
    }
    
    delete []local_content;
    
    
    dbg("test case result: %s ==> %d", __FUNCTION__, ret);
    
    return ret;
}


static
int _test_with_turn(){
    const char * caller_config_json = "{"
    //        "\"turnHost\":\"203.195.185.236\",\"turnPort\":3488,"
    "\"compCount\":2"
    ",\"turnAddrs\":["
    "{\"host\":\"203.195.185.236\",\"port\":3488}," // turn1
    "{\"host\":\"121.41.75.10\",\"port\":3488}" // turn3
    "]"
    
    "}";
    
    const char * callee_config_json = "{"
    "\"compCount\":2"
    ",\"turnAddrs\":["
    "{\"host\":\"203.195.185.236\",\"port\":348},"// turn1
    "{\"host\":\"121.41.75.10\",\"port\":348}"// turn3
    "]"
    "}";
    
    
    test_expect caller_expect;
    
    caller_expect.result = 0;
    caller_expect.local_is_host_cand = PJ_TRUE;
    caller_expect.local_is_srflx_cand = PJ_TRUE;
    caller_expect.local_is_ms_cand = PJ_FALSE;
    caller_expect.local_is_turn_addrs = PJ_TRUE;
    caller_expect.is_relay = PJ_FALSE;
    
    test_expect callee_expect;
    
    callee_expect.local_is_host_cand = PJ_TRUE;
    callee_expect.local_is_srflx_cand = PJ_TRUE;
    callee_expect.local_is_ms_cand = PJ_FALSE;
    callee_expect.local_is_turn_addrs = PJ_TRUE;
    callee_expect.result = 0;
    callee_expect.is_relay = PJ_FALSE;
    
    
    int ret = _test_with_config(caller_config_json, callee_config_json, caller_expect, callee_expect);
    return ret;
}

static
int _test_with_turn1(){
    const char * caller_config_json = "{"
    //        "\"turnHost\":\"203.195.185.236\",\"turnPort\":3488,"
    "\"compCount\":2"
    ",\"turnAddrs\":["
    "{\"host\":\"t1.easemob.com\",\"port\":3488}," // turn
//    "{\"host\":\"203.195.185.236\",\"port\":3488}," // turn1
    "{\"host\":\"121.41.75.10\",\"port\":3488}" // turn3
    "]"
    
    
    ",\"relayMS\":{"
    "\"caller\":{"
    "\"conferenceId\" : \"videop2p-79e360fc-edfc-4319-baea-c9f0356cebd9\","
    "\"serverIp\" : \"172.16.3.234\","
    "\"rcode\" : \"1406418644\","
    "\"serverPort\" : 5000,"
    "\"channelId\" : 1,"
    "\"vchannelId\" : 2"
    "},"
    
    "\"callee\":{"
    "\"conferenceId\" : \"videop2p-79e360fc-edfc-4319-baea-c9f0356cebd9\","
    "\"serverIp\" : \"172.16.3.234\","
    "\"rcode\" : \"1314571405\","
    "\"serverPort\" : 5000,"
    "\"channelId\" : 3,"
    "\"vchannelId\" : 4"
    "}"
    "}"
    
    
    "}";
    
    const char * callee_config_json = "{"
    "\"compCount\":2"
    ",\"turnAddrs\":["
    "{\"host\":\"203.195.185.236\",\"port\":348},"// turn1
    "{\"host\":\"121.41.75.10\",\"port\":348}"// turn3
    "]"
    "}";
    
    
    test_expect caller_expect;
    
    
    caller_expect.local_is_host_cand = PJ_TRUE;
    caller_expect.local_is_srflx_cand = PJ_TRUE;
    caller_expect.local_is_ms_cand = PJ_TRUE;
    caller_expect.local_is_turn_addrs = PJ_TRUE;
    caller_expect.result = 0;
    caller_expect.is_relay = PJ_FALSE;
    
    test_expect callee_expect;
    
    callee_expect.local_is_host_cand = PJ_TRUE;
    callee_expect.local_is_srflx_cand = PJ_TRUE;
    callee_expect.local_is_ms_cand = PJ_TRUE;
    callee_expect.local_is_turn_addrs = PJ_TRUE;
    callee_expect.result = 0;
    callee_expect.is_relay = PJ_FALSE;
    
    
    int ret = _test_with_config(caller_config_json, callee_config_json, caller_expect, callee_expect);
    return ret;
}

static
int _test_with_relay(){
    const char * caller_config_json = "{"
    //        "\"turnHost\":\"203.195.185.236\",\"turnPort\":3488,"
    "\"compCount\":2"
    ",\"turnAddrs\":["
    "{\"host\":\"203.195.185.236\",\"port\":3488}," // turn1
    "{\"host\":\"121.41.75.10\",\"port\":3488}" // turn3
    "]"
    
    ",\"relayMS\":{"
    "\"caller\":{"
    "\"conferenceId\" : \"videop2p-79e360fc-edfc-4319-baea-c9f0356cebd9\","
    "\"serverIp\" : \"172.16.3.234\","
    "\"rcode\" : \"1406418644\","
    "\"serverPort\" : 5000,"
    "\"channelId\" : 1,"
    "\"vchannelId\" : 2"
    "},"
    
    "\"callee\":{"
    "\"conferenceId\" : \"videop2p-79e360fc-edfc-4319-baea-c9f0356cebd9\","
    "\"serverIp\" : \"172.16.3.234\","
    "\"rcode\" : \"1314571405\","
    "\"serverPort\" : 5000,"
    "\"channelId\" : 3,"
    "\"vchannelId\" : 4"
    "}"
    "}"
    
    
    "}";
    
    const char * callee_config_json = "{"
    "\"compCount\":2"
    ",\"turnAddrs\":["
    "{\"host\":\"203.195.185.236\",\"port\":348},"// turn1
    "{\"host\":\"121.41.75.10\",\"port\":348}"// turn3
    "]"
    "}";
    
    
    test_expect caller_expect;
    
    caller_expect.result = 0;
    caller_expect.local_is_host_cand = PJ_TRUE;
    caller_expect.local_is_srflx_cand = PJ_TRUE;
    caller_expect.local_is_ms_cand = PJ_TRUE;
    caller_expect.local_is_turn_addrs = PJ_TRUE;
    caller_expect.is_relay = PJ_FALSE;
    
    test_expect callee_expect;
    
    callee_expect.local_is_host_cand = PJ_TRUE;
    callee_expect.local_is_srflx_cand = PJ_TRUE;
    callee_expect.local_is_ms_cand = PJ_TRUE;
    callee_expect.local_is_turn_addrs = PJ_TRUE;
    callee_expect.result = 0;
    callee_expect.is_relay = PJ_FALSE;
    
    
    int ret = _test_with_config(caller_config_json, callee_config_json, caller_expect, callee_expect);
    return ret;
}

static
int _test_with_force_relay(){
    const char * caller_config_json = "{"
    //        "\"turnHost\":\"203.195.185.236\",\"turnPort\":3488,"
    "\"compCount\":2"
                ","
        	"\"relayMS\":{"
            	"\"caller\":{"
        			"\"conferenceId\" : \"videop2p-79e360fc-edfc-4319-baea-c9f0356cebd9\","
        			"\"serverIp\" : \"172.16.3.234\","
        			"\"rcode\" : \"1406418644\","
        			"\"serverPort\" : 5000,"
        			"\"channelId\" : 1,"
            		"\"vchannelId\" : 2"
            	"},"
    
        		"\"callee\":{"
        			"\"conferenceId\" : \"videop2p-79e360fc-edfc-4319-baea-c9f0356cebd9\","
        			"\"serverIp\" : \"172.16.3.234\","
        			"\"rcode\" : \"1314571405\","
        			"\"serverPort\" : 5000,"
        			"\"channelId\" : 3,"
            		"\"vchannelId\" : 4"
        		"}"
        	"}"
    "}";
    
    const char * callee_config_json = "{"
    "\"compCount\":2"
    ",\"turnAddrs\":["
    "{\"host\":\"203.195.185.236\",\"port\":3488},"// turn1
    "{\"host\":\"121.41.75.10\",\"port\":3488}"// turn3
    "]"
    "}";
    
    test_expect caller_expect;
    
    caller_expect.result = 0;
    caller_expect.local_is_host_cand = PJ_TRUE;
    caller_expect.local_is_srflx_cand = PJ_FALSE;
    caller_expect.local_is_ms_cand = PJ_TRUE;
    caller_expect.local_is_turn_addrs = PJ_FALSE;
    caller_expect.is_relay = PJ_TRUE;
    
    test_expect callee_expect;
    
    callee_expect.local_is_host_cand = PJ_TRUE;
    callee_expect.local_is_srflx_cand = PJ_FALSE;
    callee_expect.local_is_ms_cand = PJ_TRUE;
    callee_expect.local_is_turn_addrs = PJ_FALSE;
    callee_expect.result = 0;
    callee_expect.is_relay = PJ_TRUE;
    
    
    
    int ret = _test_with_config(caller_config_json, callee_config_json, caller_expect, callee_expect);
    return ret;
}

static
int _test_with_force_relay_err_confUId(){
    const char * caller_config_json = "{"
    //        "\"turnHost\":\"203.195.185.236\",\"turnPort\":3488,"
    "\"compCount\":2"
    ","
    "\"relayMS\":{"
    "\"caller\":{"
    "\"conferenceId\" : \"videop2p-nonexist\","
    "\"serverIp\" : \"172.16.3.234\","
    "\"rcode\" : \"1406418644\","
    "\"serverPort\" : 5000,"
    "\"channelId\" : 1,"
    "\"vchannelId\" : 2"
    "},"
    
    "\"callee\":{"
    "\"conferenceId\" : \"videop2p-nonexist\","
    "\"serverIp\" : \"172.16.3.234\","
    "\"rcode\" : \"1314571405\","
    "\"serverPort\" : 5000,"
    "\"channelId\" : 3,"
    "\"vchannelId\" : 4"
    "}"
    "}"
    "}";
    
    const char * callee_config_json = "{"
    "\"compCount\":2"
    ",\"turnAddrs\":["
    "{\"host\":\"203.195.185.236\",\"port\":3488},"// turn1
    "{\"host\":\"121.41.75.10\",\"port\":3488}"// turn3
    "]"
    "}";
    
    test_expect caller_expect;
    
    
    caller_expect.local_is_host_cand = PJ_TRUE;
    caller_expect.local_is_srflx_cand = PJ_FALSE;
    caller_expect.local_is_ms_cand = PJ_TRUE;
    caller_expect.local_is_turn_addrs = PJ_FALSE;
    caller_expect.result = -1;
    caller_expect.is_relay = PJ_TRUE;
    
    test_expect callee_expect;
    
    callee_expect.local_is_host_cand = PJ_TRUE;
    callee_expect.local_is_srflx_cand = PJ_FALSE;
    callee_expect.local_is_ms_cand = PJ_TRUE;
    callee_expect.local_is_turn_addrs = PJ_FALSE;
    callee_expect.result = -1;
    callee_expect.is_relay = PJ_TRUE;
    
    
    
    int ret = _test_with_config(caller_config_json, callee_config_json, caller_expect, callee_expect);
    return ret;
}

static
int _test_with_err_confUId(){
    const char * caller_config_json = "{"
    //        "\"turnHost\":\"203.195.185.236\",\"turnPort\":3488,"
    "\"compCount\":2"
    ",\"turnAddrs\":["
    "{\"host\":\"203.195.185.236\",\"port\":3488}," // turn1
    "{\"host\":\"121.41.75.10\",\"port\":3488}" // turn3
    "]"
    
    ",\"relayMS\":{"
    "\"caller\":{"
    "\"conferenceId\" : \"videop2p-nonexist\","
    "\"serverIp\" : \"172.16.3.234\","
    "\"rcode\" : \"1406418644\","
    "\"serverPort\" : 5000,"
    "\"channelId\" : 1,"
    "\"vchannelId\" : 2"
    "},"
    
    "\"callee\":{"
    "\"conferenceId\" : \"videop2p-nonexist\","
    "\"serverIp\" : \"172.16.3.234\","
    "\"rcode\" : \"1314571405\","
    "\"serverPort\" : 5000,"
    "\"channelId\" : 3,"
    "\"vchannelId\" : 4"
    "}"
    "}"
    
    
    "}";
    
    const char * callee_config_json = "{"
    "\"compCount\":2"
    ",\"turnAddrs\":["
    "{\"host\":\"203.195.185.236\",\"port\":348},"// turn1
    "{\"host\":\"121.41.75.10\",\"port\":348}"// turn3
    "]"
    "}";
    
    
    test_expect caller_expect;
    
    caller_expect.result = 0;
    caller_expect.local_is_host_cand = PJ_TRUE;
    caller_expect.local_is_srflx_cand = PJ_TRUE;
    caller_expect.local_is_ms_cand = PJ_TRUE;
    caller_expect.local_is_turn_addrs = PJ_TRUE;
    caller_expect.is_relay = PJ_FALSE;
    
    test_expect callee_expect;
    
    callee_expect.local_is_host_cand = PJ_TRUE;
    callee_expect.local_is_srflx_cand = PJ_TRUE;
    callee_expect.local_is_ms_cand = PJ_TRUE;
    callee_expect.local_is_turn_addrs = PJ_TRUE;
    callee_expect.result = 0;
    callee_expect.is_relay = PJ_FALSE;
    
    
    int ret = _test_with_config(caller_config_json, callee_config_json, caller_expect, callee_expect);
    return ret;
}

static
int test_with_caller1(){
    const char * caller_config_json = "{"
    "\"compCount\":2"
    
    "}";
    
    test_expect caller_expect;
    caller_expect.result = -1;
    caller_expect.local_is_host_cand = PJ_TRUE;
    caller_expect.local_is_srflx_cand = PJ_FALSE;
    caller_expect.local_is_ms_cand = PJ_FALSE;
    caller_expect.local_is_turn_addrs = PJ_FALSE;
    
    int ret = _test_caller(caller_config_json, caller_expect);
    return ret;

}

static
int test_with_callee1(){
    const char * callee_config_json = "{"
    "\"compCount\":1"
    ",\"turnAddrs\":["
    "{\"host\":\"203.195.185.236\",\"port\":3488},"// turn1
    "{\"host\":\"121.41.75.10\",\"port\":3488}"// turn3
    "]"
    "}";
    
    const char * caller_content = "{"
    "\"candidates\":["
        "{\"component\":1,\"foundation\":\"Hac1003f0\",\"generation\":\"0\",\"id\":1,\"ip\":\"172.16.3.240\",\"network\":\"1\",\"port\":52925,\"priority\":1694498815,\"protocol\":\"udp\",\"type\":\"host\"}"
        "]"
    ",\"pwd\":\"043b1afd\""
    ",\"ufrag\":\"66b53b19\""
    "}";
    
    
    int caller_content_len = (int) strlen(caller_content);
    
    test_expect expect;
    expect.result = -1;
    expect.local_is_host_cand = PJ_TRUE;
    expect.local_is_srflx_cand = PJ_TRUE;
    expect.local_is_ms_cand = PJ_FALSE;
    expect.local_is_turn_addrs = PJ_TRUE;
    
    int ret = _test_callee(callee_config_json, caller_content, caller_content_len, expect);
    return ret;
    
}

static
int test_with_callee2(){
    const char * callee_config_json = "{"
    "\"compCount\":1"
    ",\"turnAddrs\":["
    "{\"host\":\"203.195.185.236\",\"port\":348},"// turn1
    "{\"host\":\"121.41.75.10\",\"port\":348}"// turn3
    "]"
    "}";
    
    const char * caller_content = "{"
    "\"candidates\":["
    "{\"component\":1,\"foundation\":\"Hac1003f0\",\"generation\":\"0\",\"id\":1,\"ip\":\"172.16.3.240\",\"network\":\"1\",\"port\":52925,\"priority\":1694498815,\"protocol\":\"udp\",\"type\":\"host\"}"
    "]"
    ",\"pwd\":\"043b1afd\""
    ",\"ufrag\":\"66b53b19\""
    "}";
    
    
    int caller_content_len = (int) strlen(caller_content);
    
    test_expect expect;
    expect.result = -1;
    expect.local_is_host_cand = PJ_TRUE;
    expect.local_is_srflx_cand = PJ_FALSE;
    expect.local_is_ms_cand = PJ_FALSE;
    expect.local_is_turn_addrs = PJ_TRUE;
    
    int ret = _test_callee(callee_config_json, caller_content, caller_content_len, expect);
    return ret;
    
}

static
int test_with_callee3(){
    const char * callee_config_json = "{"
    "\"compCount\":1"
    ",\"turnAddrs\":["
    "{\"host\":\"203.195.185.236\",\"port\":348},"// turn1
    "{\"host\":\"121.41.75.10\",\"port\":348}"// turn3
    "]"
    "}";
    
    const char * caller_content = "{"
    "\"candidates\":["
    "{\"component\":1,\"foundation\":\"Hac1003f0\",\"generation\":\"0\",\"id\":1,\"ip\":\"172.16.3.240\",\"network\":\"1\",\"port\":52925,\"priority\":1694498815,\"protocol\":\"udp\",\"type\":\"host\"}"
    "]"
    ",\"pwd\":\"043b1afd\""
    ",\"ufrag\":\"66b53b19\""
    ",\"turnAddrs\":[{\"host\":\"203.195.185.236\",\"port\":3488}]"
    "}";
    
    
    int caller_content_len = (int) strlen(caller_content);
    
    test_expect expect;
    expect.result = -1;
    expect.local_is_host_cand = PJ_TRUE;
    expect.local_is_srflx_cand = PJ_TRUE;
    expect.local_is_ms_cand = PJ_FALSE;
    expect.local_is_turn_addrs = PJ_TRUE;
    
    int ret = _test_callee(callee_config_json, caller_content, caller_content_len, expect);
    return ret;
    
}


class eice_addr_pair{
public:
	eice_addr_pair(){}
	eice_addr_pair(const std::string & _local_ip, int _local_port, const std::string & _remote_ip, int _remote_port, int _local_fd)
		: local_ip(_local_ip)
		, local_port(_local_port)
		, remote_ip(_remote_ip)
		, remote_port(_remote_port)
		, local_fd(_local_fd)
	{}

	std::string local_ip;
	int local_port;
	std::string remote_ip;
	int remote_port;
	int local_fd;
};

void _get_pairs_from_result(eice_t obj, const char * caller_result, std::vector<eice_addr_pair>& addr_pair){
    int ret = -1;
	std::vector<eice_addr_pair> * vec = &addr_pair;
    
    do{
        Json_em::Reader caller_reader;
        Json_em::Value caller_value;
        if (!caller_reader.parse(caller_result, caller_value)) {
            dbg("parse result fail!!!");
            ret = -1;
            break;
        }
        
        Json_em::Value pairs_value = _json_get_obj(caller_value, "pairs", Json_em::Value());
        Json_em::Value relay_pairs_value = _json_get_obj(caller_value, "relay_pairs", Json_em::Value());
        if(!pairs_value.isNull()){            
            for (int i = 0; i < pairs_value.size(); i++) {

                int port = pairs_value[i]["local"]["port"].asInt();
                int fd = eice_get_global_socket(obj, port);
                if(fd < 0) {
                    dbg("fail to get socket at %d , port %d!!!", i, port);
                    ret = -1;
                    break;
                }
                
                vec->push_back(eice_addr_pair(pairs_value[i]["local"]["ip"].asString()
                                              , pairs_value[i]["local"]["port"].asInt()
                                              , pairs_value[i]["remote"]["ip"].asString()
                                              , pairs_value[i]["remote"]["port"].asInt()
                                              , fd));
                
                int index = vec->size() - 1;
                eice_addr_pair &ap = (*vec)[index];
                dbg("store No.%d pair fd=%d, local_port=%d, remote_port=%d", index, ap.local_fd, ap.local_port, ap.remote_port);

//                dbg("get socket at %d , port=%d, fd=%d", i, port, fd);
                
                // get again to make sure removal
                fd = eice_get_global_socket(obj, port);
                if(fd > 0){
                    dbg("fail to get socket again at %d , port %d!!!", i, port);
                    ret = -1;
                    break;
                }
            }
            
            for(int i = 0; i < vec->size(); i++){                
                eice_addr_pair &ap = (*vec)[i];
                dbg("No.%d pair fd=%d, local_port=%d, remote_port=%d", i, ap.local_fd, ap.local_port, ap.remote_port);
            }
        }else{            
        }
        
        ret = 0;
    }while(0);   
}
static int _test_steal_socket(const char * caller_config_json, const char * callee_config_json
                             , test_expect& caller_expect
                             , test_expect& callee_expect){
    
    int ret = 0;
    
    //    eice_set_log_func(test_log_func);
    
    char * caller_content = new char[8*1024];
    int caller_content_len = 0;
    eice_t caller = 0;
    
    
    char * callee_content = new char[8*1024];
    int callee_content_len = 0;
    eice_t callee = 0;
    
    char * caller_result = new char[8*1024];
    int caller_result_len = 0;
    char * callee_result = new char[8*1024];
    int callee_result_len = 0;
    
    std::vector<eice_addr_pair>  caller_sockets;
    int caller_socket_num = 0;
    
    std::vector<eice_addr_pair>  callee_sockets;
    int callee_socket_num = 0;
    
    do{
        ret = eice_new_caller(caller_config_json, caller_content, &caller_content_len, &caller);        
		dbg("eice_new_caller return %d, caller=%p, content:%s.", ret, caller, caller_content);
        if(ret){
            dbg("something wrong with new caller !!!");
            break;
        }
        
        ret = caller_expect.check_content(caller_content);
        if(ret){
            dbg("check caller content fail !!!");
            break;
        }
        
        ret = eice_new_callee(callee_config_json, caller_content, caller_content_len, callee_content, &callee_content_len, &callee);
        dbg("eice_new_callee return %d", ret);
        if(ret){
            dbg("something wrong with new callee !!!");
            break;
        }
        
        ret = callee_expect.check_content(callee_content);
        if(ret){
            dbg("check callee content fail !!!");
            break;
        }
        
        
        
        ret = eice_caller_nego(caller, callee_content, callee_content_len, 0, 0);
        dbg("eice_caller_nego return %d", ret);
        if(ret){
            dbg("something wrong with new caller nego !!!");
            break;
        }
        
        
        
        while(caller_result_len == 0 || callee_result_len == 0){
            if(caller_result_len == 0){
                ret = eice_get_nego_result(caller, caller_result, &caller_result_len);
                if(ret == 0 && caller_result_len > 0){
					dbg("get caller nego result OK, caller_result:%s", caller_result);
                }
            }
            if(callee_result_len == 0){
                ret = eice_get_nego_result(callee, callee_result, &callee_result_len);
                if(ret == 0 && callee_result_len > 0){
					dbg("get callee nego result OK, callee_result:%s", callee_result);
                }
            }
            pj_thread_sleep(50);
        }
        
        eice_free(caller);
        caller = 0;
        
        eice_free(callee);
        callee = 0;
        
        ret = caller_expect.check_result(caller_result);
        if(ret != 0){
            dbg("caller expect fail !!!");
            break;
        }
        
        ret = callee_expect.check_result(callee_result);
        if(ret != 0){
            dbg("callee expect fail !!!");
            break;
        }
        
		_get_pairs_from_result(caller, caller_result, caller_sockets);
        if(caller_sockets.empty()){
            ret = -1;
            dbg("get socket from caller fail !!!");
            break;
        }
        
		_get_pairs_from_result(callee, callee_result, callee_sockets);
        if(callee_sockets.empty()){
            ret = -1;
            dbg("get socket from callee fail !!!");
            break;
        }
        
        const char * hello = "hello socket";
        int hello_len = (int)strlen(hello) + 1;
        for(int i = 0; i < caller_sockets.size(); i++){
            
            const char * remote_ip = caller_sockets[i].remote_ip.c_str();
            int remote_port = caller_sockets[i].remote_port;
            int fd = caller_sockets[i].local_fd;
            
            pj_sockaddr addr;
//            pj_str_t str;
//            pj_cstr(&str, remote_ip);
//            pj_sockaddr_init(pj_AF_INET(), &addr, &str, remote_port);
            make_addr(caller->ice_pool, remote_ip, remote_port, caller->ice_cfg.af, &addr);
            
            pj_ssize_t len = hello_len;
            pj_status_t status = pj_sock_sendto((pj_sock_t)fd, hello, &len, 0, &addr, pj_sockaddr_get_len(&addr));
            
            char errbuf[1024];
            pj_strerror(status, errbuf, sizeof(errbuf));
            dbg("send fd=%d, status=%d(%s)  ->  %s:%d", fd, status, errbuf, remote_ip, remote_port);
        }
        
        char buf[1024];
        for(int i = 0; i < callee_sockets.size(); i++){
            int fd = callee_sockets[i].local_fd;
            pj_ssize_t len = sizeof(buf);
            pj_status_t status = pj_sock_recvfrom(fd, buf, &len, 0, NULL, NULL);
            if(status != PJ_SUCCESS){
                ret = -1;
                char errbuf[1024];
                pj_strerror(status, errbuf, sizeof(errbuf));
                dbg("callee recvfrom fail, index=%d, fd=%d, status=%d (%s)", i, fd, status, errbuf);
                break;
            }
            buf[len] = '\0';
            dbg("callee recvfrom : %s", buf);
            if(strcmp(buf, hello) != 0){
                ret = -1;
                dbg("callee recvfrom data diff !!!");
                break;
            }
            
        }
        
        ret = 0;
        
    }while(0);
    
    
    
    dbg("test done");
    if(caller){
        eice_free(caller);
        caller = 0;
    }
    if(callee){
        eice_free(callee);
        callee = 0;
    }
    
    
    
    //ret = eice_new_caller(config_json, caller_content, &caller_content_len, &caller);
        
    delete []caller_result;
    delete []caller_content;
    
    delete []callee_result;
    delete []callee_content;
    
    dbg("test case result: %s ==> %d", __FUNCTION__, ret);
    
    return ret;
}

static
int _test_steal_socket_direct(){
    const char * caller_config_json = "{"
    //        "\"turnHost\":\"203.195.185.236\",\"turnPort\":3488,"
    "\"compCount\":2"
    ",\"turnAddrs\":["
    "{\"host\":\"121.41.105.183\",\"port\":3478}" // sdb-ali-hangzhou-turn1
    "]"

//    ",\"relayMS\":{"
//    "\"caller\":{"
//    "\"conferenceId\" : \"videop2p-79e360fc-edfc-4319-baea-c9f0356cebd9\","
//    "\"serverIp\" : \"172.16.3.234\","
//    "\"rcode\" : \"1406418644\","
//    "\"serverPort\" : 5000,"
//    "\"channelId\" : 1,"
//    "\"vchannelId\" : 2"
//    "},"
//
//    "\"callee\":{"
//    "\"conferenceId\" : \"videop2p-79e360fc-edfc-4319-baea-c9f0356cebd9\","
//    "\"serverIp\" : \"172.16.3.234\","
//    "\"rcode\" : \"1314571405\","
//    "\"serverPort\" : 5000,"
//    "\"channelId\" : 3,"
//    "\"vchannelId\" : 4"
//    "}"
//    "}"


    "}";

    const char * callee_config_json = "{"
    "\"compCount\":2"
    ",\"turnAddrs\":["
    "{\"host\":\"121.41.105.183\",\"port\":3478}" // sdb-ali-hangzhou-turn1
    "]"
    "}";


    test_expect caller_expect;


    caller_expect.local_is_host_cand = PJ_TRUE;
    caller_expect.local_is_srflx_cand = PJ_TRUE;
    caller_expect.local_is_ms_cand = PJ_FALSE;
    caller_expect.local_is_turn_addrs = PJ_TRUE;
    caller_expect.result = 0;
    caller_expect.is_relay = PJ_FALSE;

    test_expect callee_expect;

    callee_expect.local_is_host_cand = PJ_TRUE;
    callee_expect.local_is_srflx_cand = PJ_TRUE;
    callee_expect.local_is_ms_cand = PJ_FALSE;
    callee_expect.local_is_turn_addrs = PJ_TRUE;
    callee_expect.result = 0;
    callee_expect.is_relay = PJ_FALSE;


    int ret = _test_steal_socket(caller_config_json, callee_config_json, caller_expect, callee_expect);
    if(ret != 0) return ret;
    
    return ret;
}



static void test_json(){
    Json_em::FastWriter writer;
    Json_em::Value root_value;
    Json_em::Value value;
    root_value["ttt"] = "{}";
    
    std::string * str = new std::string(writer.write(root_value));
    dbg("str=%s", str->c_str());
}

int eice_test(){
    int ret = 0;
    eice_init();
    do{
        

        
//        ret = test_with_caller1();
//        if(ret) break;
//        
//        ret = test_with_callee1();
//        if(ret) break;
//        
//        ret = test_with_callee2();
//        if(ret) break;
//        
//        ret = test_with_callee3();
//        if(ret) break;
//        
//        ret = _test_with_turn();
//        if(ret) break;
//        
//        ret = _test_with_relay();
//        if(ret) break;
//        
//        ret = _test_with_force_relay();
//        if(ret) break;
//        
//        ret = _test_with_err_confUId();
//        if(ret) break;
//        
//        ret = _test_with_force_relay_err_confUId();
//        if(ret) break;
        
//        ret = _test_with_turn1();
//        if(ret) break;

        ret = _test_steal_socket_direct();
        if(ret) break;
        
        ret = 0;
    }while(0);
    
    dbg("test result: final ==> %d", ret);
    eice_exit();
    return ret;
}






