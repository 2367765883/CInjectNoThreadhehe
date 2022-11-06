#include "ApcInject.h"


typedef PVOID(__stdcall* MGetThreadTeb)(PETHREAD pEthread);
typedef UCHAR BYTE;

BOOLEAN SkipApcThread(PETHREAD pthread)
{
	PUCHAR pteb64 = (PUCHAR)PsGetThreadTeb(pthread);
	//DbgPrint("pteb64 : %p", pteb64);
	//DbgPrint("pteb64 + 0x78 : %p", pteb64 + 0x78);

	//if (!MmIsAddressValid(pteb64)) {
	//	DbgPrint("pteb64 ��ַ����");
	//}
	//else
	//{
	//	DbgPrint("pteb64 ��ַ��ȷ");
	//}

	if (!pteb64) {
		return TRUE;
	}

	

	if (*(PULONG64)(pteb64 + 0x78)!=0) {
		return TRUE;
	}

	if (*(PULONG64)(pteb64 + 0x2c8) == 0) {
		return TRUE;
	}

	if (*(PULONG64)(pteb64 + 0x58) == 0) {
		return TRUE;
	}


	return FALSE;
}


PETHREAD FindThreadInProcess(HANDLE pid)
{
	PEPROCESS pEpro = NULL;
	NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;
	ntStatus = PsLookupProcessByProcessId(pid, &pEpro);

	if (!NT_SUCCESS(ntStatus))
	{
		ObDereferenceObject(pEpro);
		return NULL;
	}

	KAPC_STATE kapc;
	//����
	KeStackAttachProcess(pEpro,&kapc);

	PETHREAD pretthreadojb = NULL, ptempthreadobj = NULL;

	PLIST_ENTRY plisthead = NULL;

	PLIST_ENTRY plistflink = NULL;

	int i = 0;

	plisthead = (PLIST_ENTRY)((PUCHAR)pEpro + 0x30);

	plistflink = plisthead->Flink;

	//����
	for (plistflink; plistflink != plisthead; plistflink = plistflink->Flink)
	{
		ptempthreadobj = (PETHREAD)((PUCHAR)plistflink - 0x2f8);

		HANDLE threadId = PsGetThreadId(ptempthreadobj);

		DbgPrint("�߳�ID: %d", threadId);

		

		if (!MmIsAddressValid(ptempthreadobj)) {
			continue;
		}

		i++;

		if (!SkipApcThread(ptempthreadobj)) {
			pretthreadojb = ptempthreadobj;
			break;
		}
		
	}

	KeUnstackDetachProcess(&kapc);
	ObDereferenceObject(pEpro); //������
	return pretthreadojb;
}

VOID PKKERNEL_ROUTINE_CALLBACK(
	IN struct _KAPC* Apc,
	IN OUT PKNORMAL_ROUTINE* NormalRoutine,
	IN OUT PVOID* NormalContext,
	IN OUT PVOID* SystemArgument1,
	IN OUT PVOID* SystemArgument2
) {
	//�ͷ�Apc
	ExFreePool(Apc);
	DbgPrint("APC�ص�ִ�У�");
}

bool APCExecuteFunction(PEPROCESS process , PVOID func,ULONG64 modulebase) {

	//��ѡһ�����ʵ��߳�
	PETHREAD thread = FindThreadInProcess(PsGetProcessId(process));
	if (!thread) {
		DbgPrint("��ѡ�߳�ʧ��");
		return false;
	}

	PRKAPC kapc = (PRKAPC)ExAllocatePool(NonPagedPool, sizeof(KAPC));
	KeInitializeApc(
		kapc,
		thread,
		OriginalApcEnvironment,
		PKKERNEL_ROUTINE_CALLBACK,
		NULL,
		(PKNORMAL_ROUTINE)func,		//(PKNORMAL_ROUTINE)fun_addr
		UserMode,
		(PVOID)modulebase
	);

	BOOLEAN res = KeInsertQueueApc(kapc
		, (PVOID)DLL_PROCESS_ATTACH, NULL,			//PKKERNEL_ROUTINE �ص��Ĳ���
		IO_NO_INCREMENT			//���ȼ�
	);



	if (!res) {

		return false;
		//DbgPrint("�ں�APC����ɹ���");
	}

	DbgPrint("�ں�APC����ɹ���,�߳�ID: %d", PsGetThreadId(thread));

	return true;
}