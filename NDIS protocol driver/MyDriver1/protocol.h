#pragma warning(push)
#pragma warning(disable:4201)
#include<basetsd.h>
#include<ntifs.h>
#include<ndis.h>
#include<wdm.h>
#include<wdmsec.h>
#pragma warning(pop)
#pragma warning(disable:4127)
#pragma warning(disable:4100)
#define MAX_PACKET_POOL_SIZE 0x0000FFFF//最大64K
#define MIN_PACKET_POOL_SIZE 0x000000FF//最小256字节
#define Tranverse16(X)                 ((((UINT16)(X) & 0xff00) >> 8) |(((UINT16)(X) & 0x00ff) << 8))
typedef struct 
{
	PNDIS_PACKET pack[20];
	int packnum;
	PVOID buffer[20];
	NDIS_HANDLE sendpacketpool;
	NDIS_HANDLE recvpacketpool;
	NDIS_HANDLE recvbufferpool;
	NDIS_STATUS status;
	NDIS_EVENT eve;
	NDIS_EVENT unbindeve;
	int contextno;
}adapercontext, *padapercontext;
typedef struct _NPROT_SEND_PACKET_RSVD
{
	PIRP                    pIrp;
	ULONG                   RefCount;

} NPROT_SEND_PACKET_RSVD, *PNPROT_SEND_PACKET_RSVD;
typedef struct _NPROT_RECV_PACKET_RSVD
{
	PNDIS_BUFFER            pOriginalBuffer;    

} NPROT_RECV_PACKET_RSVD, *PNPROT_RECV_PACKET_RSVD;
typedef struct _GLOBAL
{
	PDRIVER_OBJECT driver;
	PDEVICE_OBJECT dev;
	NDIS_HANDLE ndisprotocolhandle;
	NDIS_HANDLE bindinghandle[10];
	int bindinghandlenum;
	padapercontext context[10];
	int contextnum;
}*PGLOBAL, GLOBAL;
GLOBAL Globals;
NDIS_STATUS ndisprotDoRequest(
	padapercontext context,
	NDIS_REQUEST_TYPE type,
	NDIS_OID Oid,
	PVOID inforbuffer,
	ULONG bufferlength,
	PULONG pbyteprocessd);
typedef struct _ETHeader //以太网数据帧头部结构
{
	UCHAR dhost[6]; //目的MAC地址
	UCHAR shost[6]; //源MAC地址
	USHORT type; //下层协议类型，如IP(ETHERTYPE_IP)，ARP（ETHERTYPE_ARP)等
}ETHeader, *PETHeader;
VOID packetadd(padapercontext context, PNDIS_PACKET pack);
VOID clearallpacket(padapercontext context);
VOID
NdisProtOpenAdapterComplete(
IN NDIS_HANDLE                  ProtocolBindingContext,
IN NDIS_STATUS                  Status,
IN NDIS_STATUS                  OpenErrorCode
)
{
	DbgPrint("1\n");
}
VOID
NdisProtCloseAdapterComplete(
IN NDIS_HANDLE                  ProtocolBindingContext,
IN NDIS_STATUS                  Status
)
{
	padapercontext context = (padapercontext)ProtocolBindingContext;
	DbgPrint("enter close adaper complete\n");
	NdisSetEvent(&context->unbindeve);
	context->status = Status;
}
VOID
NdisProtSendComplete(
IN NDIS_HANDLE                  ProtocolBindingContext,
IN PNDIS_PACKET                 pNdisPacket,
IN NDIS_STATUS                  Status
)
{
	DbgPrint("1\n");
}
VOID
NdisProtTransferDataComplete(
IN NDIS_HANDLE                  ProtocolBindingContext,
IN PNDIS_PACKET                 pNdisPacket,
IN NDIS_STATUS                  TransferStatus,
IN UINT                         BytesTransferred
)
{
	DbgPrint("enter transfer complete\n");
	PNDIS_BUFFER prebuf;
	padapercontext context = ProtocolBindingContext;
	PNDIS_BUFFER buf=((PNPROT_RECV_PACKET_RSVD)&(pNdisPacket->ProtocolReserved[0]))->pOriginalBuffer;
	NdisUnchainBufferAtBack(pNdisPacket, &prebuf);
	NdisChainBufferAtBack(pNdisPacket, buf);
	DbgPrint("buffer:%x\n", (char*)(context->buffer[context->packnum])+14);
	if (prebuf != NULL)
	{
		NdisFreeBuffer(prebuf);
	}
	packetadd(context, pNdisPacket);
}
VOID
NdisProtResetComplete(
IN NDIS_HANDLE                  ProtocolBindingContext,
IN NDIS_STATUS                  Status
)
{
	DbgPrint("1\n");
}
VOID
NdisProtRequestComplete(
IN NDIS_HANDLE                  ProtocolBindingContext,
IN PNDIS_REQUEST                pNdisRequest,
IN NDIS_STATUS                  Status
)
{
	DbgPrint("enter request complete\n");
	padapercontext context = (padapercontext)ProtocolBindingContext;
	context->status = Status;
	NdisSetEvent(&context->eve);
}
NDIS_STATUS
NdisProtReceive(
IN NDIS_HANDLE                              ProtocolBindingContext,
IN NDIS_HANDLE                              MacReceiveContext,
__in_bcount(HeaderBufferSize) IN PVOID      pHeaderBuffer,
IN UINT                                     HeaderBufferSize,
__in_bcount(LookaheadBufferSize) IN PVOID   pLookaheadBuffer,
IN UINT                                     LookaheadBufferSize,
IN UINT                                     PacketSize
)
{
	NDIS_STATUS ndsta;
	PNDIS_BUFFER ndisbuf;
	PNDIS_BUFFER ndisbuf1;
	PNDIS_PACKET ndispacket;
	UINT bytetransfer;
	padapercontext context = ProtocolBindingContext;
	DbgPrint("enter receive\n");
	DbgPrint("headsize:%d\n", HeaderBufferSize);
	DbgPrint("headbuffer:0x%x\n", pHeaderBuffer);
	if (HeaderBufferSize == 14)
	{
		PETHeader et = pHeaderBuffer;
		DbgPrint("source mac:%2x-%2x-%2x-%2x-%2x-%2x\n", et->shost[0], et->shost[1], et->shost[2],
			et->shost[3], et->shost[4], et->shost[5]);
		DbgPrint("dest mac:%2x-%2x-%2x-%2x-%2x-%2x\n", et->dhost[0], et->dhost[1], et->dhost[2],
			et->dhost[3], et->dhost[4], et->dhost[5]);
		DbgPrint("type:0x%x\n", Tranverse16(et->type));
		PVOID revbuf;
		NdisAllocateMemoryWithTag(&revbuf, PacketSize + HeaderBufferSize, 0);
		context->buffer[context->packnum] = revbuf;
		NdisAllocatePacket(&ndsta, &ndispacket, context->recvpacketpool);
		NdisAllocateBuffer(&ndsta, &ndisbuf, context->recvbufferpool, revbuf,PacketSize+HeaderBufferSize );
		NdisAllocateBuffer(&ndsta, &ndisbuf1, context->recvbufferpool, (char*)revbuf + HeaderBufferSize, PacketSize);
		NdisMoveMappedMemory(revbuf, pHeaderBuffer, HeaderBufferSize);
		NdisChainBufferAtBack(ndispacket, ndisbuf1);
		((PNPROT_RECV_PACKET_RSVD)&(ndispacket->ProtocolReserved[0]))->pOriginalBuffer = ndisbuf;
		NdisTransferData(&ndsta, Globals.bindinghandle[context->contextno], MacReceiveContext, 0, PacketSize, ndispacket, &bytetransfer);
		if (ndsta != NDIS_STATUS_PENDING)
		{
			NdisProtTransferDataComplete(context, ndispacket, ndsta, bytetransfer);
		}
	}
	return NDIS_STATUS_SUCCESS;
}
VOID
NdisProtReceiveComplete(
IN NDIS_HANDLE                  ProtocolBindingContext
)
{
	DbgPrint("enter receive complete\n");
}
VOID
NdisProtStatus(
IN NDIS_HANDLE                          ProtocolBindingContext,
IN NDIS_STATUS                          GeneralStatus,
__in_bcount(StatusBufferSize) IN PVOID  StatusBuffer,
IN UINT                                 StatusBufferSize
)
{
	DbgPrint("enter status\n");
}
VOID
NdisProtStatusComplete(
IN NDIS_HANDLE                  ProtocolBindingContext
)
{
	DbgPrint("enter status complete\n");
}
VOID
NdisProtBindAdapter(
OUT PNDIS_STATUS                pStatus,
IN NDIS_HANDLE                  BindContext,
IN PNDIS_STRING                 pDeviceName,
IN PVOID                        SystemSpecific1,
IN PVOID                        SystemSpecific2
)
{
	DbgBreakPoint();
	DbgPrint("enter bind adapter\n");
	NDIS_MEDIUM mediumarray[1] = { NdisMedium802_3 };
	NDIS_STATUS sta;
	NDIS_STATUS errorcode;
	UINT seletemediumundex;
	padapercontext context;
	NdisAllocateMemoryWithTag(&context, sizeof(adapercontext), 0);
	context->contextno = Globals.contextnum;
	context->packnum = 0;
	NdisZeroMemory(context->pack, sizeof(context->pack));
	do
	{
		NdisAllocatePacketPoolEx(&sta, &context->sendpacketpool, MIN_PACKET_POOL_SIZE, MAX_PACKET_POOL_SIZE-MIN_PACKET_POOL_SIZE, sizeof(NPROT_SEND_PACKET_RSVD));
		DbgPrint("0x%x\n", sta);
		if (sta != NDIS_STATUS_SUCCESS)
		{
			break;
		}
		NdisAllocatePacketPoolEx(&sta, &context->recvpacketpool, MIN_PACKET_POOL_SIZE, MAX_PACKET_POOL_SIZE - MIN_PACKET_POOL_SIZE, sizeof(NPROT_RECV_PACKET_RSVD));
		DbgPrint("0x%x\n", sta);
		if (sta != NDIS_STATUS_SUCCESS)
		{
			break;
		}
		NdisAllocateBufferPool(&sta, &context->recvbufferpool, 200);
		DbgPrint("0x%x\n", sta);
		if (sta != NDIS_STATUS_SUCCESS)
		{
			break;
		}
		NdisOpenAdapter(&sta, &errorcode, &Globals.bindinghandle[Globals.bindinghandlenum], &seletemediumundex, &mediumarray[0], 1, 
			Globals.ndisprotocolhandle, (NDIS_HANDLE)context, pDeviceName, 0, NULL);
		DbgPrint("0x%x\n", sta);
		DbgPrint("0x%x\n", errorcode);
		DbgPrint("%wZ\n", pDeviceName);
		Globals.context[Globals.contextnum] = context;
		Globals.contextnum++;
		Globals.bindinghandlenum++;
	} while (FALSE);
}

VOID
NdisProtUnbindAdapter(
OUT PNDIS_STATUS                pStatus,
IN NDIS_HANDLE                  ProtocolBindingContext,
IN NDIS_HANDLE                  UnbindContext
)
{
	DbgBreakPoint();
	DbgPrint("enter unbind\n");
	ULONG PacketFilter = 0;
	ULONG Byteread;
	int num;
	NDIS_STATUS sta;
	padapercontext context = (padapercontext)ProtocolBindingContext;
	num = context->contextno;
	NdisInitializeEvent(&context->unbindeve);
	ndisprotDoRequest(context, NdisRequestSetInformation, OID_GEN_CURRENT_PACKET_FILTER,
		&PacketFilter, sizeof(PacketFilter), &Byteread);
	ndisprotDoRequest(context, NdisRequestSetInformation, OID_802_3_MULTICAST_LIST, NULL, 0, &Byteread);
	NdisCloseAdapter(&sta, Globals.bindinghandle[num]);
	if (sta == NDIS_STATUS_PENDING)
	{
		DbgPrint("unbind pending\n");
		NdisWaitEvent(&context->unbindeve,0);
		sta = context->status;
	}
	DbgPrint("0x%x\n", sta);
}
INT
NdisProtReceivePacket(
IN NDIS_HANDLE                  ProtocolBindingContext,
IN PNDIS_PACKET                 pNdisPacket
)
{
	DbgPrint("enter receive packet\n");
	padapercontext context = ProtocolBindingContext;
	PNDIS_BUFFER pNdisbuffer;
	PNDIS_BUFFER buffer;
	PNDIS_PACKET pack;
	NDIS_STATUS ndissta;
	PVOID buf;
	UINT firstsize;
	UINT totalsize;
	UINT bytecopy;
	NdisGetFirstBufferFromPacketSafe(pNdisPacket, &pNdisbuffer, &buf, &firstsize, &totalsize, NormalPagePriority);
	ndissta=NdisAllocateMemoryWithTag(&buf, totalsize, 0);
	context->buffer[context->packnum] = buf;
	NdisAllocateBuffer(&ndissta, &buffer, context->recvbufferpool, buf, totalsize);
	NdisAllocatePacket(&ndissta, &pack, context->recvpacketpool);
	NdisChainBufferAtBack(pack, buffer);
	NdisCopyFromPacketToPacketSafe(pack, 0, totalsize, pNdisPacket, 0, &bytecopy, NormalPagePriority);
	packetadd(context, pack);
	return 0;
}
NDIS_STATUS
NdisProtPnPEventHandler(
IN NDIS_HANDLE                  ProtocolBindingContext,
IN PNET_PNP_EVENT               pNetPnPEvent
)
{
	DbgPrint("rec pnp\n");
	return NDIS_STATUS_SUCCESS;
}
NDIS_STATUS ndisprotDoRequest(
	padapercontext context,
	NDIS_REQUEST_TYPE type,
	NDIS_OID Oid,
	PVOID inforbuffer,
	ULONG bufferlength,
	PULONG pbyteprocessd)
{
	NDIS_STATUS sta;
	NDIS_REQUEST request;
	NdisInitializeEvent(&context->eve);
	switch (type)
	{
	case NdisRequestQueryInformation:
		request.RequestType = NdisRequestQueryInformation;
		request.DATA.QUERY_INFORMATION.Oid = Oid;
		request.DATA.QUERY_INFORMATION.InformationBuffer = inforbuffer;
		request.DATA.QUERY_INFORMATION.InformationBufferLength = bufferlength;
		break;
	case NdisRequestSetInformation:
		request.RequestType = NdisRequestSetInformation;
		request.DATA.SET_INFORMATION.Oid = Oid;
		request.DATA.SET_INFORMATION.InformationBuffer = inforbuffer;
		request.DATA.SET_INFORMATION.InformationBufferLength = bufferlength;
		break;
	default:
		DbgPrint("error\n");
		return NDIS_STATUS_FAILURE;
		break;
	}
	NdisRequest(&sta, Globals.bindinghandle[context->contextno], &request);
	if (sta == NDIS_STATUS_PENDING)
	{
		DbgPrint("pending...\n");
		NdisWaitEvent(&context->eve, 0);
		sta = context->status;
	}
	if (sta == NDIS_STATUS_SUCCESS)
	{
		*pbyteprocessd = (type == NdisRequestQueryInformation) ?
			request.DATA.QUERY_INFORMATION.BytesWritten :
			request.DATA.SET_INFORMATION.BytesRead;
	}
	DbgPrint("request return:0x%x\n", sta);
	return sta;
}
VOID packetadd(padapercontext context, PNDIS_PACKET pack)
{
	if (context->packnum == 20)
	{
		clearallpacket(context);
	}
	context->pack[context->packnum] = pack;
	context->packnum++;
	context->packnum = 0;
}
VOID clearallpacket(padapercontext context)
{
	do
	{
		for (int j = 0; j < 20; j++)
		{
			if (context->pack[j] != NULL)
			{
				PNDIS_BUFFER buf;
				NdisUnchainBufferAtBack(context->pack[j], &buf);
				NdisFreeBuffer(buf);
				NdisFreePacket(context->pack[j]);
				ExFreePool(context->buffer[j]);
			}
		}
	} while (FALSE);
}