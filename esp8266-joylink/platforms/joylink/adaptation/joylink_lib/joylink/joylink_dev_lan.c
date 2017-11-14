#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#ifndef ESP_8266
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <errno.h>
#include <ifaddrs.h>
#include <netdb.h>
#else
#include "esp_common.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#endif
#include "joylink.h"
#include "joylink_utils.h"
#include "joylink_packets.h"
#include "joylink_crypt.h"
#include "joylink_json.h"
#include "joylink_extern.h"
#include "joylink_sub_dev.h"
#include "joylink_join_packet.h"
#ifdef ESP_8266
#define perror os_printf
#include "crc.h"
#else
#include "auth/crc.h"
#endif
extern int joylink_server_upload_req();
//extern void joylink_upload_event(joylink_connect_status_t evt);

/**
 * brief: 1 get packet head and opt
 *        2 cut big packet as 1024
 *        3 join phead, opt and paload
 *        4 crc
 *        5 send
 */
#define JL_MAX_CUT_PACKET_LEN  (1024)
void
joylink_send_big_pkg(struct sockaddr_in *sin_recv, socklen_t addrlen)
{
    int ret = -1;
    int len;
    int i;
    int offset = 0;

	JLPacketHead_t *pt = (JLPacketHead_t*)_g_pdev->send_p;
    short res = pt->crc;

    int total = (int)(pt->payloadlen/JL_MAX_CUT_PACKET_LEN) 
        + (pt->payloadlen%JL_MAX_CUT_PACKET_LEN? 1:0);

    for(i = 1; i <= total; i ++){
        if(i == total){
            len = pt->payloadlen%JL_MAX_CUT_PACKET_LEN;
        }else{
            len = JL_MAX_CUT_PACKET_LEN;
        }
        
        offset = 0;
        bzero(_g_pdev->send_buff, sizeof(_g_pdev->send_buff));

        memcpy(_g_pdev->send_buff, _g_pdev->send_p, sizeof(JLPacketHead_t) + pt->optlen);
        offset  = sizeof(JLPacketHead_t) + pt->optlen;

        memcpy(_g_pdev->send_buff + offset, 
                _g_pdev->send_p + offset + JL_MAX_CUT_PACKET_LEN * (i -1), len);

        pt = (JLPacketHead_t*)_g_pdev->send_buff; 
        pt->total = total;
        pt->index = i;
        pt->payloadlen = len;
        pt->crc = CRC16(_g_pdev->send_buff + sizeof(JLPacketHead_t), pt->optlen + pt->payloadlen);
        pt->reserved = (unsigned char)res;

        ret = sendto(_g_pdev->lan_socket, 
                _g_pdev->send_buff,
                len + sizeof(JLPacketHead_t) + pt->optlen, 0,
                (SOCKADDR*)sin_recv, addrlen);

        if(ret < 0){
            log_error("send error");
        }
        log_info("send to:%s:ret:%d", _g_pdev->jlp.ip, ret);
    }

    if(NULL != _g_pdev->send_p){
        free(_g_pdev->send_p);
        _g_pdev->send_p = NULL;
    }
}

void
joylink_printf_info(uint8_t *pStr, uint16_t ret)
{
	 /***************os_printf******************/
    	int i;
		log_debug("send packet:");       
		for (i=0; i<ret; i++){
			if(i<(sizeof(JLPacketHead_t)+(pStr[5]<<8|pStr[4])))
				os_printf("%02x ",pStr[i]);
			else
				//os_printf("%c",pStr[i]);
				os_printf("%02x ",pStr[i]);
		}
		os_printf("\n");
		JLPacketHead_t* pPack = (JLPacketHead_t*)pStr;
		log_info(" magic:0x%08x\n optlen:%d\n payloadlen:%d\n version:%d\n type:%d\n total:%d\n index:%d\n enctype:%d\n",
					pPack->magic,pPack->optlen,pPack->payloadlen,pPack->version,pPack->type,pPack->total,pPack->index,pPack->enctype);
	/*********************************/
}
static void
joylink_proc_lan_scan(uint8_t *src, struct sockaddr_in *sin_recv, socklen_t addrlen)
{
    int ret = -1;
    int len;
    DevScan_t scan;
    bzero(&scan, sizeof(scan));

    if(E_RET_OK == joylink_parse_scan(&scan, (const char *)src)){
    	if (scan.uuid[0] == 0){
			strcpy(scan.uuid,_g_pdev->jlp.uuid);
			scan.type = 0;
			log_debug("scan info re-assign");
    	}

        len = joylink_packet_lan_scan_rsp(&scan);
    }

    if(len > 0 && len < JL_MAX_PACKET_LEN){ 
        if(len < JL_MAX_PACKET_LEN){
        	
            ret = sendto(_g_pdev->lan_socket, 
                    _g_pdev->send_buff,
                    len, 0,
                    (SOCKADDR*)sin_recv, addrlen);
            if(ret < 0){
                perror("send error");
            }
        }else{
            joylink_send_big_pkg(sin_recv, addrlen);
            log_info("SEND PKG IS TOO BIG:ip:%sret:%d", _g_pdev->jlp.ip, ret);
        }
        log_info("send to:%s:ret:%d", _g_pdev->jlp.ip, ret);
    }else{
        log_error("packet error ret:%d", ret);
    }
}

static void
joylink_proc_lan_write_key(uint8_t *src, struct sockaddr_in *sin_recv, socklen_t addrlen)
{
    int ret = -1;
    DevEnable_t de;
    bzero(&de, sizeof(de));

    joylink_parse_lan_write_key(&de, (const char*)src + 4);

    log_info("-->feedid:%s:accesskey:%s", de.feedid, de.accesskey);
    log_info("-->localkey:%s", de.localkey);
    log_info("-->joylink_server:%s", de.joylink_server);

    strcpy(_g_pdev->jlp.feedid, de.feedid);
    strcpy(_g_pdev->jlp.accesskey, de.accesskey);
    strcpy(_g_pdev->jlp.localkey, de.localkey);

	//get joylink server from lan acce
    joylink_util_cut_ip_port(de.joylink_server,
            _g_pdev->jlp.joylink_server,
            &_g_pdev->jlp.server_port);

    _g_pdev->jlp.isUsed = 1;
	//save info to flash
    joylink_dev_set_attr_jlp(&_g_pdev->jlp); 

    int len = joylink_packet_lan_write_key_rsp();
    if(len > 0 && len < JL_MAX_PACKET_LEN){ 
        ret = sendto(_g_pdev->lan_socket, _g_pdev->send_buff, 
                len, 0, (SOCKADDR*)sin_recv, addrlen);

        if(ret < 0){
            log_error("send error ret:%d", ret);
            perror("send error");
        }
    }else{
        log_error("packet error ret:%d", ret);
    }
}

static void
joylink_proc_lan_json_ctrl(uint8_t *json_cmd, struct sockaddr_in *sin_recv, socklen_t addrlen)
{
    int ret = -1;
    int len = 0;
    
    time_t tt = time(NULL);
//	char data[JL_MAX_PACKET_LEN];
    /* Reuse memory */
    char *data = (char *)g_recBuffer;
	char feedid[JL_MAX_FEEDID_LEN];

	bzero(feedid, JL_MAX_FEEDID_LEN);
    bzero(data,JL_MAX_PACKET_LEN);
    int is_fdid = joylink_parse_json_ctrl(feedid, (char*)json_cmd);//find out "feedid" in pMsg

    ret = joylink_dev_lan_json_ctrl((char *)json_cmd);//control device in json 
	memcpy(data, &tt, 4);

    ret = joylink_dev_get_json_snap_shot(data + 4, JL_MAX_PACKET_LEN - 4, ret, (E_RET_OK==is_fdid)?feedid:NULL);

    log_info("rsp data:%s:len:%d", data + 4 , ret);

    len = joylink_encrypt_lan_basic(_g_pdev->send_buff, JL_MAX_PACKET_LEN,
            ET_ACCESSKEYAES, PT_SCRIPTCONTROL,
            (uint8_t*)_g_pdev->jlp.localkey, (const uint8_t*)data, ret + 4);

    if(len > 0 && len < JL_MAX_PACKET_LEN){ 
        ret = sendto(_g_pdev->lan_socket, _g_pdev->send_buff, len, 0,
                (SOCKADDR*)sin_recv, addrlen);

        if(ret < 0){
            log_error(" 1 send error ret:%d", ret);
        }else{
            log_info("rsp to:%s:len:%d", _g_pdev->jlp.ip, ret);
        }
    }else{
        log_error("packet error ret:%d", ret);
    }

    joylink_server_upload_req();
} 

static void
joylink_proc_lan_script_ctrl(uint8_t *src, struct sockaddr_in *sin_recv, socklen_t addrlen)
{
    int ret = -1;
    int len = 0;
//    char data[JL_MAX_PACKET_LEN];
    /* reuse memory*/
    char *data = (char *)g_recBuffer;
    JLContrl_t ctr;
    bzero(&ctr, sizeof(ctr));
	bzero(data, JL_MAX_PACKET_LEN);

    if(-1 == joylink_dev_script_ctrl((const char *)src, &ctr, 0)){
        return;
    }
    ret = joylink_packet_script_ctrl_rsp(data, JL_MAX_PACKET_LEN, &ctr, 0);
    log_info("rsp data:%s:len:%d", data + 12, ret);
    len = joylink_encrypt_lan_basic(_g_pdev->send_buff, JL_MAX_PACKET_LEN,
            ET_ACCESSKEYAES, PT_SCRIPTCONTROL,
            (uint8_t*)_g_pdev->jlp.localkey, (const uint8_t*)data, ret);

    if(len > 0 && len < JL_MAX_PACKET_LEN){ 
        ret = sendto(_g_pdev->lan_socket, _g_pdev->send_buff, len, 0,
                (SOCKADDR*)sin_recv, addrlen);

        if(ret < 0){
            log_error(" 1 send error ret:%d", ret);
        }else{
            log_info("rsp to:%s:len:%d", _g_pdev->jlp.ip, ret);
        }
    }else{
        log_error("packet error ret:%d", ret);
    }
    joylink_server_upload_req();
}

E_JLRetCode_t 
joylink_proc_lan_rsp_send(uint8_t* data, int len,  struct sockaddr_in *sin_recv, socklen_t addrlen)
{
    int ret = E_RET_ERROR;
    int en_len = 0;
    en_len = joylink_encrypt_lan_basic(_g_pdev->send_buff, JL_MAX_PACKET_LEN,
            ET_ACCESSKEYAES, PT_SCRIPTCONTROL,
            (uint8_t*)_g_pdev->jlp.localkey, (const uint8_t*)data, len);

    if(en_len > 0 && en_len < JL_MAX_PACKET_LEN){ 
        ret = sendto(_g_pdev->lan_socket, _g_pdev->send_buff, en_len, 0,
                (SOCKADDR*)sin_recv, addrlen);

        if(ret < 0){
            log_error("send error ret:%d", ret);
        }else{
            log_info("rsp to:%s:len:%d", _g_pdev->jlp.ip, ret);
        }
    }else{
        log_error("packet error ret:%d", ret);
    }

    joylink_server_upload_req();

    return ret;
}

void
joylink_proc_lan()
{
    int ret;
    //uint8_t recBuffer[JL_MAX_PACKET_LEN];
    /*uint8_t recPainText[JL_MAX_PACKET_LEN] = { 0 };*/
    //uint8_t recPainBuffer[JL_MAX_PACKET_LEN];
    uint8_t *recPainText = NULL;
    JLPacketHead_t* pHead = NULL;

	bzero(g_recBuffer,sizeof(g_recBuffer));
	bzero(g_recPainText,sizeof(g_recPainText));

    struct sockaddr_in sin_recv;
    JLPacketParam_t param;

    bzero(&sin_recv, sizeof(sin_recv));
    bzero(&param, sizeof(param));
    socklen_t addrlen = sizeof(SOCKADDR);

    ret = recvfrom(_g_pdev->lan_socket, g_recBuffer, 
            sizeof(g_recBuffer), 0, (SOCKADDR*)&sin_recv, &addrlen);       
    if(ret == -1){
        goto RET;
    }
    joylink_util_get_ipstr(&sin_recv, _g_pdev->jlp.ip);
    log_debug("before dencypt: %s", g_recBuffer);
    ret = joylink_dencypt_lan_req(&param, g_recBuffer, ret, g_recPainText, JL_MAX_PACKET_LEN * 2);
    log_debug("after dencypt: %s", g_recPainText);
    if (ret <= 0){
        goto RET;
    }
    pHead = (JLPacketHead_t*)g_recBuffer;
    if(pHead->total > 1){
        log_info("PACKET WAIT TO JOIN!!!!!!:IP:%s", _g_pdev->jlp.ip);
        if(E_RET_ALL_DATA_HIT ==  joylink_join_pkg_add_data(_g_pdev->jlp.ip,
                    pHead->reserved, pHead, (char*)g_recPainText + 4, ret - 4)){

            recPainText = (uint8_t*)joylink_join_pkg_join_data(
                    _g_pdev->jlp.ip,
                    pHead->reserved,
                    &ret);
        }else{
            log_info("PACKET WAIT TO JOIN!!!!!!:IP:%s", _g_pdev->jlp.ip);
        }
    }else{
        recPainText = g_recPainText;
    }

    if(NULL == recPainText){
        goto RET;
    }
    switch (param.type){
        case PT_SCAN:
            if(param.version == 1){
                log_info("PT_SCAN:%s (Scan->Type:%d, Version:%d)", 
                        _g_pdev->jlp.ip, param.type, param.version);
                joylink_proc_lan_scan(recPainText, &sin_recv, addrlen);
            }
            break;
        case PT_WRITE_ACCESSKEY:
            if(param.version == 1){
                log_info("PT_WRITE_ACCESSKEY");
                log_info("write key org:%s", recPainText + 4);
                joylink_proc_lan_write_key(recPainText, &sin_recv, addrlen);
            }
            break;
        case PT_JSONCONTROL:
            if(param.version == 1){
                log_info("JSon Control->%s", recPainText + 4);
                joylink_proc_lan_json_ctrl(recPainText + 4, &sin_recv, addrlen);
            }
            break;
        case PT_SCRIPTCONTROL:
            if(param.version == 1){
                log_debug("SCRIPT Control->%s", recPainText + 12);
                joylink_proc_lan_script_ctrl(recPainText, &sin_recv, addrlen);
            }
            break;
        case PT_SUB_AUTH:
            if(param.version == 1){
                log_info("PT_SUB_AUTH");
                log_debug("Control->%s", recPainText + 4);
                joylink_proc_lan_sub_auth(recPainText + 4, &sin_recv, addrlen);
            }
            break;
        case PT_SUB_LAN_JSON:
            if(param.version == 1){
                log_debug("Control->%s", recPainText + 4);
                joylink_proc_lan_json_ctrl(recPainText + 4, &sin_recv, addrlen);
            }
            break;
        case PT_SUB_LAN_SCRIPT:
            if(param.version == 1){
                log_info("PT_SUB_LAN_SCRIPT");
                log_debug("localkey->%s", _g_pdev->jlp.localkey);
                log_debug("Control->%s", recPainText + 12 + 32);
                joylink_proc_lan_sub_script_ctrl(recPainText, ret,
                            &sin_recv, addrlen);
            }
            break;
        case PT_SUB_ADD:
            if(param.version == 1){
                log_info("PT_SUB_ADD");
                log_debug("Sub add->%s", recPainText + 4);
                joylink_proc_lan_sub_add(recPainText + 4, &sin_recv, addrlen);
            }
            break;
        default:
            break;
    }

RET:
    /*if(NULL != pHead */
            /*&& pHead->total > 1*/
            /*&& NULL != recPainText){*/
            /*free(recPainText);*/
    /*}*/
    return;
}
