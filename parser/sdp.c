/* 
	HCIDump - HCI packet analyzer	
	Copyright (C) 2000-2001 Maxim Krasnyansky <maxk@qualcomm.com>

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License version 2 as
	published by the Free Software Foundation;

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
	OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF THIRD PARTY RIGHTS.
	IN NO EVENT SHALL THE COPYRIGHT HOLDER(S) AND AUTHOR(S) BE LIABLE FOR ANY CLAIM,
	OR ANY SPECIAL INDIRECT OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER
	RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
	NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE
	USE OR PERFORMANCE OF THIS SOFTWARE.

	ALL LIABILITY, INCLUDING LIABILITY FOR INFRINGEMENT OF ANY PATENTS, COPYRIGHTS,
	TRADEMARKS OR OTHER RIGHTS, RELATING TO USE OF THIS SOFTWARE IS DISCLAIMED.
*/

/*
	SDP parser.
	Copyright (C) 2001 Ricky Yuen <ryuen@qualcomm.com>
*/

/*
 * $Id$
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <asm/types.h> 
#include <netinet/in.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/l2cap.h>

#include "parser.h"
#include "sdp.h"

static inline void print_de(int, struct frame*);

sdp_siz_idx_lookup_table_t sdp_siz_idx_lookup_table[] = {
	{ 0, 1  }, /* Size index = 0 */
	{ 0, 2  }, /*              1 */
	{ 0, 4  }, /*              2 */
	{ 0, 8  }, /*              3 */
	{ 0, 16 }, /*              4 */
	{ 1, 1  }, /*              5 */
	{ 1, 2  }, /*              6 */
	{ 1, 4  }, /*              7 */
};

sdp_uuid_nam_lookup_table_t sdp_uuid_nam_lookup_table[] = {
	{ SDP_UUID_SDP,                      "SDP"          },
	{ SDP_UUID_UDP,                      "UDP"          },
	{ SDP_UUID_RFCOMM,                   "RFCOMM"       },
	{ SDP_UUID_TCP,                      "TCP"          },
	{ SDP_UUID_TCS_BIN,                  "TCS-BIN"      },
	{ SDP_UUID_TCS_AT,                   "TCS-AT"       },
	{ SDP_UUID_OBEX,                     "OBEX"         },
	{ SDP_UUID_IP,                       "IP"           },
	{ SDP_UUID_FTP,                      "FTP"          },
	{ SDP_UUID_HTTP,                     "HTTP"         },
	{ SDP_UUID_WSP,                      "WSP"          },
	{ SDP_UUID_L2CAP,                    "L2CAP"        },
	{ SDP_UUID_SERVICE_DISCOVERY_SERVER, "SDServer"     },
	{ SDP_UUID_BROWSE_GROUP_DESCRIPTOR,  "BrwsGrpDesc"  },
	{ SDP_UUID_PUBLIC_BROWSE_GROUP,      "PubBrwsGrp"   },
	{ SDP_UUID_SERIAL_PORT,              "SP"           },
	{ SDP_UUID_LAN_ACCESS_PPP,           "LAN"          },
	{ SDP_UUID_DIALUP_NETWORKING,        "DUN"          },
	{ SDP_UUID_IR_MC_SYNC,               "IRMCSync"     },
	{ SDP_UUID_OBEX_OBJECT_PUSH,         "OBEXObjPush"  },
	{ SDP_UUID_OBEX_FILE_TRANSFER,       "OBEXObjTrnsf" },
	{ SDP_UUID_IR_MC_SYNC_COMMAND,       "IRMCSyncCmd"  },
	{ SDP_UUID_HEADSET,                  "Headset"      },
	{ SDP_UUID_CORDLESS_TELEPHONY,       "CordlessTel"  },
	{ SDP_UUID_INTERCOM,                 "Intercom"     },
	{ SDP_UUID_FAX,                      "Fax"          },
	{ SDP_UUID_HEADSET_AUDIO_GATEWAY,    "AG"           },
	{ SDP_UUID_PNP_INFORMATION,          "PNPInfo"      },
	{ SDP_UUID_GENERIC_NETWORKING,       "Networking"   },
	{ SDP_UUID_GENERIC_FILE_TRANSFER,    "FileTrnsf"    },
	{ SDP_UUID_GENERIC_AUDIO,            "Audio"        },
	{ SDP_UUID_GENERIC_TELEPHONY,        "Telephony"    }
};

sdp_attr_id_nam_lookup_table_t sdp_attr_id_nam_lookup_table[] = {
	{ SDP_ATTR_ID_SERVICE_RECORD_HANDLE,             "SrvRecHndl"         },
	{ SDP_ATTR_ID_SERVICE_CLASS_ID_LIST,             "SrvClassIDList"     },
	{ SDP_ATTR_ID_SERVICE_RECORD_STATE,              "SrvRecState"        },
	{ SDP_ATTR_ID_SERVICE_SERVICE_ID,                "SrvID"              },
	{ SDP_ATTR_ID_PROTOCOL_DESCRIPTOR_LIST,          "ProtocolDescList"   },
	{ SDP_ATTR_ID_BROWSE_GROUP_LIST,                 "BrwGrpList"         },
	{ SDP_ATTR_ID_LANGUAGE_BASE_ATTRIBUTE_ID_LIST,   "LangBaseAttrIDList" },
	{ SDP_ATTR_ID_SERVICE_INFO_TIME_TO_LIVE,         "SrvInfoTimeToLive"  },
	{ SDP_ATTR_ID_SERVICE_AVAILABILITY,              "SrvAvail"           },
	{ SDP_ATTR_ID_BLUETOOTH_PROFILE_DESCRIPTOR_LIST, "BTProfileDescList"  },
	{ SDP_ATTR_ID_DOCUMENTATION_URL,                 "DocURL"             },
	{ SDP_ATTR_ID_CLIENT_EXECUTABLE_URL,             "ClientExeURL"       },
	{ SDP_ATTR_ID_ICON_10,                           "Icon10"             },
	{ SDP_ATTR_ID_ICON_URL,                          "IconURL"            },
	{ SDP_ATTR_ID_SERVICE_NAME,                      "SrvName"            },
	{ SDP_ATTR_ID_SERVICE_DESCRIPTION,               "SrvDesc"            },
	{ SDP_ATTR_ID_PROVIDER_NAME,                     "ProviderName"       },
	{ SDP_ATTR_ID_VERSION_NUMBER_LIST,               "VersionNumList"     },
	{ SDP_ATTR_ID_GROUP_ID,                          "GrpID"              },
	{ SDP_ATTR_ID_SERVICE_DATABASE_STATE,            "SrvDBState"         },
	{ SDP_ATTR_ID_SERVICE_VERSION,                   "SrvVersion"         }
};

static inline __u8 get_u8(struct frame *frm)
{
	__u8 *u8_ptr = frm->ptr;
	frm->ptr += 1;
	frm->len -= 1;
	return *u8_ptr;
}

static inline __u16 get_u16(struct frame *frm)
{
	__u16 *u16_ptr = frm->ptr;
	frm->ptr += 2;
	frm->len -= 2;
	return ntohs(*u16_ptr);
}

static inline __u32 get_u32(struct frame *frm)
{
	__u32 *u32_ptr = frm->ptr;
	frm->ptr += 4;
	frm->len -= 4;
	return ntohl(*u32_ptr);
}


static inline char* get_uuid_name(int uuid)
{
	int i;

	for (i = 0; i < SDP_UUID_NAM_LOOKUP_TABLE_SIZE; i++)
	{
		if (sdp_uuid_nam_lookup_table[i].uuid == uuid)
		{
			return sdp_uuid_nam_lookup_table[i].name;
		}
	}

	return 0;
}

static inline char* get_attr_id_name(int attr_id)
{
	int i;

	for (i = 0; i < SDP_ATTR_ID_NAM_LOOKUP_TABLE_SIZE; i++)
	{
		if (sdp_attr_id_nam_lookup_table[i].attr_id == attr_id)
		{
			return sdp_attr_id_nam_lookup_table[i].name;
		}
	}

	return 0;
}

static inline __u8 parse_de_hdr(struct frame *frm, int* n)
{
	__u8	de_hdr = get_u8(frm);
	__u8	de_type = de_hdr >> 3;
	__u8	siz_idx = de_hdr & 0x07;

	/* Get the number of bytes */
	if (sdp_siz_idx_lookup_table[siz_idx].addl_bits) {
		switch(sdp_siz_idx_lookup_table[siz_idx].num_bytes) {
		case 1:
			*n = get_u8(frm); break;
		case 2:
			*n = get_u16(frm); break;
		case 4:
			*n = get_u32(frm); break;
		}
	} else {
		*n = sdp_siz_idx_lookup_table[siz_idx].num_bytes;
	}

	return de_type;
}

static inline void print_des(__u8 de_type, int level, int n, struct frame *frm)
{
	int len = frm->len;

	while (len - frm->len < n ) {
		print_de(level, frm);
	}
}

static inline void print_int(__u8 de_type, int level, int n, struct frame *frm)
{
	__u64 val;

	switch(de_type) {
	case SDP_DE_UINT:
		printf(" uint");
		break;
	case SDP_DE_INT:
		printf(" int");
		break;
	case SDP_DE_BOOL:
		printf(" bool");
		break;
	}

	switch(n) {
	case 1: /* 8-bit */
		val = get_u8(frm);
		break;
	case 2: /* 16-bit */
		val = get_u16(frm);
		break;
	case 4: /* 32-bit */
		val = get_u32(frm);
		break;
	case 8: /* 64-bit */
		/* Not supported yet */
		val = 0;
		break;
	default: /* syntax error */
		printf(" err");
		frm->ptr += n;
		frm->len -= n;
		return;
	}

	printf(" 0x%llx", val);
}

static inline void print_uuid(int n, struct frame *frm)
{
	__u32 uuid = 0;
	char* s;

	switch(n) {
	case 2: /* 16-bit UUID */
		uuid = get_u16(frm);
		break;
	case 4: /* 32_bit UUID */
		uuid = get_u32(frm);
		break;
	case 16: /* 128-bit UUID */
		uuid = get_u32(frm);
		frm->ptr += 12;
		frm->len -= 12;
		break;
	default: /* syntax error */
		printf(" *err*");
		frm->ptr += n;
		frm->len -= n;
		return;
	}

	printf(" 0x%x", uuid);
	if ((s = get_uuid_name(uuid)) != 0)
	{
		printf(" (%s)", s);
	}
}

static inline void print_string(int n, struct frame *frm)
{
	char	*s;

	printf(" str");
	if ((s = malloc(n + 1))) {
		s = frm->ptr;
		s[n] = '\0';
		printf(" \"%s\"", s);
	} else {
		perror("Can't allocate string buffer");
	}

	frm->ptr += n;
	frm->len -= n;
}

static inline void print_de(int level, struct frame *frm)
{
	int  n;
	__u8 de_type = parse_de_hdr(frm, &n);

	switch(de_type) {
	case SDP_DE_NULL:
		printf("null");
		break;
	case SDP_DE_UINT:
	case SDP_DE_INT:
	case SDP_DE_BOOL:
		print_int(de_type, level, n, frm);
		break;
	case SDP_DE_UUID:
		print_uuid(n, frm);
		break;
	case SDP_DE_STRING:
		print_string(n, frm);
		break;
	case SDP_DE_SEQ:
	case SDP_DE_ALT:
		print_des(de_type, ++level, n, frm);
		break;
	case SDP_DE_URL:
		break;
	}
}


static inline void print_srv_srch_pat(int level, struct frame *frm)
{
	int len = frm->len;
	int n1;
	int n2;

	p_indent(level, frm->in);
	printf("pat");

	if (parse_de_hdr(frm, &n1) == SDP_DE_SEQ) {
		while (len - frm->len < n1 ) {
			if (parse_de_hdr(frm,&n2) == SDP_DE_UUID) {
				print_uuid(n2, frm);
			} else {
				printf("\nERROR: Unexpected syntax\n");
				raw_dump(level, frm);
			}
		}
		printf("\n");
	} else {
		printf("\nERROR: Unexpected syntax\n");
		raw_dump(level, frm);
	}
}


static inline void print_attr_id_list(int level, struct frame *frm)
{
	__u16 attr_id;
	__u32 attr_id_range;
	int   n1;
	int   n2;
	int   len = frm->len;

	p_indent(level, frm->in);
	printf("aid(s)");

	if (parse_de_hdr(frm, &n1) == SDP_DE_SEQ) {
		while (len - frm->len < n1 ) {
			/* Print AttributeID */
			if (parse_de_hdr(frm, &n2) == SDP_DE_UINT) {
				switch(n2) {
				case 2:
					attr_id = get_u16(frm);
					printf(" 0x%x (%s)", attr_id, get_attr_id_name(attr_id));
					break;
				case 4:
					attr_id_range = get_u32(frm);
					printf(" 0x%x--0x%x",
							(attr_id_range >> 16),
							(attr_id_range & 0x00FF));
					break;
				}
			} else {
				printf("\nERROR: Unexpected syntax\n");
				raw_dump(level, frm);
			}
		}
		printf("\n");
	} else {
		printf("\nERROR: Unexpected syntax\n");
		raw_dump(level, frm);
	}
}


static inline void print_attr_list(int level, struct frame *frm)
{
	__u16 attr_id;
	int   n1;
	int   n2;
	int   len = frm->len;

	if (parse_de_hdr(frm, &n1) == SDP_DE_SEQ) {
		while (len - frm->len < n1 ) {
			/* Print AttributeID */
			if ((parse_de_hdr(frm, &n2) == SDP_DE_UINT) &&
				 (n2 == sizeof(attr_id))) {
				attr_id = get_u16(frm);
				p_indent(level, 0);
				printf("aid 0x%x (%s)\n", attr_id, get_attr_id_name(attr_id));

				/* Print AttributeValue */
				p_indent(++level, 0);
				print_de(level, frm);
			} else {
				printf("\nERROR: Unexpected syntax\n");
				raw_dump(level, frm);
				break;
			}
		}
		printf("\n");
	} else {
		printf("\nERROR: Unexpected syntax\n");
		raw_dump(level, frm);
	}
}


static inline void print_attr_lists(int level, struct frame *frm)
{
	int   n;
	int   cnt = 0;
	int   len = frm->len;

	if (parse_de_hdr(frm, &n) == SDP_DE_SEQ) {
		while (len - frm->len < n ) {
			p_indent(level, 0);
			printf("srv rec #%d\n", cnt++);
			print_attr_list(level+1, frm);
		}
	} else {
		printf("\nERROR: Unexpected syntax\n");
		raw_dump(level, frm);
	}
}


static inline void err_rsp(int level, __u16 tid, __u16 len, struct frame *frm)
{
	printf("SDP Error Rsp: tid 0x%x len 0x%x\n", tid, len);

	p_indent(++level, 0);
   printf("ec 0x%x info ", get_u16(frm));
	if (frm->len > 0) {
		raw_dump(0, frm);
	} else {
		printf("none\n");
	}
}


static inline void ss_req(int level, __u16 tid, __u16 len, struct frame *frm)
{
	printf("SDP SS Req: tid 0x%x len 0x%x\n", tid, len);

	/* Parse ServiceSearchPattern */
	print_srv_srch_pat(++level, frm);

	/* Parse MaximumServiceRecordCount */
	p_indent(level, 0);
	printf("max 0x%x\n", get_u16(frm));
}

static inline void ss_rsp(int level, __u16 tid, __u16 len, struct frame *frm)
{
	register int i;
	__u16 cur_srv_rec_cnt = get_u16(frm); /* Parse CurrentServiceRecordCount */
	__u16 tot_srv_rec_cnt = get_u16(frm); /* Parse TotalServiceRecordCount */

	printf("SDP SS Rsp: tid 0x%x len 0x%x\n", tid, len);

	p_indent(++level, 0);
	printf("cur 0x%x tot 0x%x", cur_srv_rec_cnt, tot_srv_rec_cnt);

	/* Parse service record handle(s) */
	if (cur_srv_rec_cnt > 0) {
		printf(" hndl");
		for (i = 0; i < cur_srv_rec_cnt; i++) {
			printf(" 0x%x", get_u32(frm));
		}
	}
	printf("\n");
}

static inline void sa_req(int level, __u16 tid, __u16 len, struct frame *frm)
{
	printf("SDP SA Req: tid 0x%x len 0x%x\n", tid, len);

	/* Parse ServiceRecordHandle */
	p_indent(++level, 0);
	printf("hndl 0x%x\n", get_u32(frm));

	/* Parse MaximumAttributeByteCount */
	p_indent(level, 0);
	printf("max 0x%x\n", get_u16(frm));

	/* Parse ServiceSearchPattern */
	print_attr_id_list(level, frm);
}

static inline void sa_rsp(int level, __u16 tid, __u16 len, struct frame *frm)
{
	printf("SDP SA Rsp: tid 0x%x len 0x%x\n", tid, len);

	/* Parse AttributeByteCount */
	p_indent(++level, 0);
	printf("cnt 0x%x\n", get_u16(frm));

	/* Parse AttributeList */
	print_attr_list(level, frm);
}

static inline void ssa_req(int level, __u16 tid, __u16 len, struct frame *frm)
{
	printf("SDP SSA Req: tid 0x%x len 0x%x\n", tid, len);

	/* Parse ServiceSearchPattern */
	print_srv_srch_pat(++level, frm);

	/* Parse MaximumAttributeByteCount */
	p_indent(level, 0);
   printf("max 0x%x\n", get_u16(frm));

	/* Parse AttributeList */
	print_attr_id_list(level, frm);
}

static inline void ssa_rsp(int level, __u16 tid, __u16 len, struct frame *frm)
{
	printf("SDP SSA Rsp: tid 0x%x len 0x%x\n", tid, len);

	/* Parse AttributeByteCount */
	p_indent(++level, 0);
	printf("cnt 0x%x\n", get_u16(frm));

	/* Parse AttributeLists */
	print_attr_lists(level, frm);
}

void sdp_dump(int level, struct frame *frm)
{
	sdp_pdu_hdr *hdr = frm->ptr;
 	__u16 tid = ntohs(hdr->tid);
	__u16 len = ntohs(hdr->len);

	frm->ptr += SDP_PDU_HDR_SIZE;
	frm->len -= SDP_PDU_HDR_SIZE;

	p_indent(++level, frm->in);

	switch(hdr->pid) {
	case SDP_ERROR_RSP:
		err_rsp(level, tid, len, frm);
		break;
	case SDP_SERVICE_SEARCH_REQ:
		ss_req(level, tid, len, frm);
		break;
	case SDP_SERVICE_SEARCH_RSP:
		ss_rsp(level, tid, len, frm);
		break;
	case SDP_SERVICE_ATTR_REQ:
		sa_req(level, tid, len, frm);
		break;
	case SDP_SERVICE_ATTR_RSP:
		sa_rsp(level, tid, len, frm);
		break;
	case SDP_SERVICE_SEARCH_ATTR_REQ:
		ssa_req(level, tid, len, frm);
		break;
	case SDP_SERVICE_SEARCH_ATTR_RSP:
		ssa_rsp(level, tid, len, frm);
		break;
	default:
		printf("ERROR: Unknown PDU ID: 0x%x\n", hdr->pid);
		raw_dump(++level, frm);
		return;
	}

	if (hdr->pid != SDP_ERROR_RSP)
	{
		/* Parse ContinuationState */
		if (*(__u8*) frm->ptr)	{
			p_indent(++level, frm->in);
			printf("cont ");
			raw_dump(0, frm);
		}
	}
}