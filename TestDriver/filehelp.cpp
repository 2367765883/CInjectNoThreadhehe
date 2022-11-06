#include "filehelp.h"

NTSTATUS ReadFile(const WCHAR* path, PVOID * buffer, PULONG64 size)
{
	OBJECT_ATTRIBUTES obj = { 0 };
	HANDLE readHandle = NULL;
	IO_STATUS_BLOCK ioStackblock = { 0 };

	UNICODE_STRING filepath = { 0 };
	RtlUnicodeStringInit(&filepath, path);

	//��ʼ��OBJECT_ATTRIBUTES
	InitializeObjectAttributes(&obj, &filepath,
		OBJ_KERNEL_HANDLE|OBJ_CASE_INSENSITIVE,
		NULL,NULL
		);

	//����һ��handle

	NTSTATUS creafileStatus = ZwCreateFile(
		&readHandle,	//�ļ����
		GENERIC_READ,   //��Ȩ��
		&obj,			//��ʼ����OBJECT_ATTRIBUTES
		&ioStackblock,	//�ýṹ�����������״̬���й������������������Ϣ
		NULL,			//�����򸲸ǵ��ļ��ĳ�ʼ�����С�����ֽ�Ϊ��λ��
		FILE_ATTRIBUTE_NORMAL,	//��Щ��־��ʾ�ڴ����򸲸��ļ�ʱҪ���õ��ļ�����
		FILE_SHARE_READ,		//����Ȩ��
		FILE_OPEN_IF,			//ָ�����ļ����ڻ򲻴���ʱҪִ�еĲ���
		FILE_NON_DIRECTORY_FILE | FILE_RANDOM_ACCESS | FILE_SYNCHRONOUS_IO_NONALERT,
		NULL,
		0
	);

	if (!NT_SUCCESS(creafileStatus)) {
		DbgPrint("ZwCreateFileʧ��");
		return STATUS_UNSUCCESSFUL;
	}

	//��ȡ�ļ�����
	FILE_STANDARD_INFORMATION fsi = {0};
	NTSTATUS QueryInformationStatus = ZwQueryInformationFile(readHandle,
		&ioStackblock,
		&fsi,
		sizeof(FILE_STANDARD_INFORMATION),
		FileStandardInformation);

	if (!NT_SUCCESS(QueryInformationStatus)) {
		DbgPrint("ZwQueryInformationFile ��ȡ�ļ���Сʧ��");
		return STATUS_UNSUCCESSFUL;
	}

	//���뻺����
	SIZE_T filesize = (LONG)fsi.EndOfFile.QuadPart;
	PVOID filebuffer = ExAllocatePool(NonPagedPool, filesize);
	memset(filebuffer,0, filesize);


	NTSTATUS ReadFilestatus =  ZwReadFile(
		readHandle,		//�ļ����
		NULL,NULL,NULL,
		&ioStackblock,	//�ýṹ�����������״̬���й�������Ķ�ȡ��������Ϣ
		filebuffer,	//������
		filesize,	//��С
		NULL,
		0
	);

	if (!NT_SUCCESS(ReadFilestatus)) {
		DbgPrint("ZwReadFile ʧ��");
		return STATUS_UNSUCCESSFUL;
	}

	*buffer = filebuffer;
	*size = filesize;

	ZwClose(readHandle);

	return STATUS_SUCCESS;
}
