

#ifndef MSTESTER_H_
#define MSTESTER_H_

#ifdef __cplusplus
extern "C"{
#endif

typedef struct mstester_st* mstester_t;
typedef void    (*mstester_on_message_t)(mstester_t obj, void * cbContext,
		const char * msg, int msg_len);
    
typedef void mstester_log_func(int level, const char *data, int len);


int mstester_new(const char* config, mstester_t * pobj);
void mstester_free(mstester_t obj);
    
int mstester_start(mstester_t obj);

void mstester_stop(mstester_t obj);
    
int mstester_get_status(mstester_t obj, char * content, int * p_content_len);

void check_register_pj_thread();
    
int test_mstester();
    
#ifdef __cplusplus
}
#endif

#endif /* end */
