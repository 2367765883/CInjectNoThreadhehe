#include "PeHelper.h"
#include "utils.h"

#define MAX(a,b) (a>b?a:b)
#ifdef _WIN64
typedef ULONGLONG	QDWORD;
typedef PULONGLONG	PQDWORD;
#else
typedef DWORD	QDWORD;
typedef PDWORD	PQDWORD;
#endif

typedef WORD *PWORD;
typedef DWORD * PDWORD;
typedef int		 BOOL;

typedef INT_PTR(FAR _stdcall* FARPROC)();

#define CRLF "\r\n"
#define printf(str,...) DbgPrint(str CRLF, __VA_ARGS__)


// ���������С
static DWORD AlignedSize(DWORD dwOrigin, DWORD dwAlignment)
{
	
	return (dwOrigin + dwAlignment - 1) / dwAlignment * dwAlignment;
}

PVOID RemoteAllocateMemory(size_t size) {
	PVOID addr =  ExAllocatePool(NonPagedPool, size);
	memset(addr, 0 , size);
	return addr;
}

int  wcscasecmp(const  wchar_t* cs, const  wchar_t* ct)
{
	while (towlower(*cs) == towlower(*ct))
	{
		if (*cs == 0)
			return 0;
		cs++;
		ct++;
	}
	return towlower(*cs) - towlower(*ct);
}

//��ȡģ���ַ
NTSTATUS GetModelBase(HANDLE pid, LPCWSTR moudelName, PVOID* Dllbase) {
	PEPROCESS process = NULL;
	//��ȡ���̶���
	PsLookupProcessByProcessId(pid, &process);

	if (process == NULL)
	{
		DbgPrint("��ȡ���̶���ʧ��,PID:%d", pid);
		return FALSE;
	}

	PPEB64 peb = (PPEB64)PsGetProcessPeb(process);


	//UNICODE_STRING moduleName;
	if (peb <= 0)
	{
		DbgPrint("��ȡpebʧ��");
		ObDereferenceObject(process);
		return STATUS_UNSUCCESSFUL;
	}

	KAPC_STATE state;
	KeStackAttachProcess(process, &state);

	DbgPrint("���ң�%s�ĵ�ַ", moudelName);

	// InLoadOrderLinks will have main executable first, ntdll.dll second, kernel32.dll
	for (PLIST_ENTRY pListEntry = peb->Ldr->InLoadOrderLinks.Flink; pListEntry != &peb->Ldr->InLoadOrderLinks; pListEntry = pListEntry->Flink)
	{
		if (!pListEntry)
			continue;

		PLDR_DATA_TABLE_ENTRY module_entry = CONTAINING_RECORD(pListEntry, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);

		__try
		{

			//if (RtlCompareUnicodeString(&module_entry->BaseDllName, &ustr, TRUE) == 0)
			//wchar_t str[64] = L"R5Apex.exe";
			int result = wcscasecmp((PWCH)module_entry->BaseDllName.Buffer, moudelName);

			if (result == 0)
				*Dllbase = module_entry->DllBase;

			DbgPrint("ģ���ַ%llx,ģ����%wZ,�Ƚ�%d", module_entry->DllBase, module_entry->BaseDllName, result);
		}
		__except (1)
		{
			DbgPrint("��ȡʧ��");
			//�����
			KeUnstackDetachProcess(&state);
			//���ں˶�����������1
			ObDereferenceObject(process);
			return FALSE;
		}

	}

	KeUnstackDetachProcess(&state);
	ObDereferenceObject(process);

	DbgPrint("�ҵ���ģ���ַ%llx", *Dllbase);

	return STATUS_SUCCESS;
}

//��ȡ��������
ULONG_PTR MyGetProcAddress(
	PVOID hModule,    // handle to DLL module  
	LPCSTR lpProcName,   // function name  
	HANDLE pid
)
{
	int i = 0;
	char* pRet = NULL;
	PIMAGE_DOS_HEADER pImageDosHeader = NULL;
	PIMAGE_NT_HEADERS pImageNtHeader = NULL;
	PIMAGE_EXPORT_DIRECTORY pImageExportDirectory = NULL;

	pImageDosHeader = (PIMAGE_DOS_HEADER)hModule;
	pImageNtHeader = (PIMAGE_NT_HEADERS)((ULONG_PTR)hModule + pImageDosHeader->e_lfanew);
	pImageExportDirectory = (PIMAGE_EXPORT_DIRECTORY)((ULONG_PTR)hModule + pImageNtHeader->OptionalHeader.DataDirectory
		[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);

	DWORD dwExportRVA = pImageNtHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
	DWORD dwExportSize = pImageNtHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].Size;

	DWORD* pAddressOfFunction = (DWORD*)(pImageExportDirectory->AddressOfFunctions + (ULONG_PTR)hModule);
	DWORD* pAddressOfNames = (DWORD*)(pImageExportDirectory->AddressOfNames + (ULONG_PTR)hModule);
	DWORD dwNumberOfNames = (DWORD)(pImageExportDirectory->NumberOfNames);
	DWORD dwBase = (DWORD)(pImageExportDirectory->Base);

	WORD* pAddressOfNameOrdinals = (WORD*)(pImageExportDirectory->AddressOfNameOrdinals + (ULONG_PTR)hModule);

	//����ǲ�һ���ǰ���ʲô��ʽ����������or������ţ����麯����ַ��  
	DWORD dwName = (DWORD)(ULONG_PTR)lpProcName;
	if ((dwName & 0xFFFF0000) == 0)
	{
		goto xuhao;
	}

	for (i = 0; i < (int)dwNumberOfNames; i++)
	{
		char* strFunction = (char*)(pAddressOfNames[i] + (ULONG_PTR)hModule);
		if (strcmp(strFunction, (char*)lpProcName) == 0)
		{
			pRet = (char*)(pAddressOfFunction[pAddressOfNameOrdinals[i]] + (ULONG_PTR)hModule);
			goto _exit11;
		}
	}
	//�����ͨ������ŵķ�ʽ���麯����ַ��  
xuhao:
	if (dwName < dwBase || dwName > dwBase + pImageExportDirectory->NumberOfFunctions - 1)
	{
		return 0;
	}
	pRet = (char*)(pAddressOfFunction[dwName - dwBase] + (ULONG_PTR)hModule);
_exit11:
	//�жϵõ��ĵ�ַ��û��Խ��  
	if ((ULONG_PTR)pRet<dwExportRVA + (ULONG_PTR)hModule || (ULONG_PTR)pRet > dwExportRVA + (ULONG_PTR)hModule + dwExportSize)
	{
		return (ULONG_PTR)pRet;
	}
	char pTempDll[100] = { 0 };
	char pTempFuction[100] = { 0 };
	strcpy(pTempDll, pRet);
	char* p = strchr(pTempDll, '.');
	if (!p)
	{
		return (ULONG_PTR)pRet;
	}
	*p = 0;
	strcpy(pTempFuction, p + 1);
	strcat(pTempDll, ".dll");
	PVOID h = NULL;


	UNICODE_STRING WmouduleName = {0};
	ANSIToUNCODESTRING(pTempDll, &WmouduleName);
	GetModelBase(pid, WmouduleName.Buffer,&h);
	RtlFreeUnicodeString(&WmouduleName);

	if (h == NULL)
	{
		return (ULONG_PTR)pRet;
	}

	return MyGetProcAddress(h, pTempFuction,pid);
}

// �ض���
BOOL DoRelocation(ULONG_PTR lpMemModule, PUCHAR virtualBase)
{
	PIMAGE_DOS_HEADER lpDosHeader = (PIMAGE_DOS_HEADER)lpMemModule;
	PIMAGE_NT_HEADERS lpNtHeader = (PIMAGE_NT_HEADERS)(lpMemModule + lpDosHeader->e_lfanew);
	QDWORD dwDelta = (QDWORD)(lpMemModule - lpNtHeader->OptionalHeader.ImageBase);

	if (0 == dwDelta || 0 == lpNtHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size) {
		return TRUE;
	}

	ULONG index = 1;

	DWORD dwRelocationOffset = lpNtHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress;
	PIMAGE_BASE_RELOCATION lpBaseRelocation = (PIMAGE_BASE_RELOCATION)(lpMemModule + dwRelocationOffset);
	while (0 != lpBaseRelocation->VirtualAddress)
	{
		DWORD dwRelocationSize = (lpBaseRelocation->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
		for (DWORD i = 0; i < dwRelocationSize; i++)
		{
			WORD wRelocationValue = *((PWORD)(lpMemModule + dwRelocationOffset + sizeof(IMAGE_BASE_RELOCATION) + i * sizeof(WORD)));
			WORD wRelocationType = wRelocationValue >> 12;

			if (IMAGE_REL_BASED_DIR64 == wRelocationType && sizeof(PULONGLONG) == sizeof(PQDWORD))
			{
				PQDWORD lpAddress = (PQDWORD)(lpMemModule + lpBaseRelocation->VirtualAddress + (wRelocationValue & 4095));
				*lpAddress = (ULONG_PTR)virtualBase + (*lpAddress - lpMemModule) + dwDelta;
				printf("My�ض���: %d \r\n", index++);
			}
			else if (IMAGE_REL_BASED_HIGHLOW == wRelocationType && sizeof(PDWORD) == sizeof(PQDWORD))
			{
				PQDWORD lpAddress = (PQDWORD)(lpMemModule + lpBaseRelocation->VirtualAddress + (wRelocationValue & 4095));
				*lpAddress += dwDelta;
			}
			else if (IMAGE_REL_BASED_ABSOLUTE != wRelocationType)
			{
				return FALSE;
			}
		}

		dwRelocationOffset += lpBaseRelocation->SizeOfBlock;
		lpBaseRelocation = (PIMAGE_BASE_RELOCATION)(lpMemModule + dwRelocationOffset);
	}

	return TRUE;
}

// ��䵼���
static BOOL FillRavAddress(ULONG_PTR lpMemModule, PVOID virtualBase, HANDLE pid)
{
	PIMAGE_DOS_HEADER lpDosHeader = (PIMAGE_DOS_HEADER)lpMemModule;
	PIMAGE_NT_HEADERS lpNtHeader = (PIMAGE_NT_HEADERS)(lpMemModule + lpDosHeader->e_lfanew);

	if (0 == lpNtHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size) {
		return TRUE;
	}

	DWORD i = 0;
	PIMAGE_IMPORT_DESCRIPTOR lpImportDescriptor = (PIMAGE_IMPORT_DESCRIPTOR)(lpMemModule + lpNtHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
	while (0 != lpImportDescriptor[i].Name)
	{
		DWORD j = 0;
		QDWORD realIAT = 0;
		//HMODULE hModule = NULL;

		LPCSTR lpModuleName = (LPCSTR)(lpMemModule + lpImportDescriptor[i].Name);	// ģ����

		PVOID moudlebase = NULL;

		UNICODE_STRING WmouduleName = {0};
		ANSIToUNCODESTRING(lpModuleName,&WmouduleName);
		GetModelBase(pid,WmouduleName.Buffer,&moudlebase);
		RtlFreeUnicodeString(&WmouduleName);

		printf("ģ���� : %s ����ַ�� %p \r\n", lpModuleName, moudlebase);

		DWORD dwFirstThunk = lpImportDescriptor[i].OriginalFirstThunk ? lpImportDescriptor[i].OriginalFirstThunk : lpImportDescriptor[i].FirstThunk;	// IAT��

		while (0 != (realIAT = ((PQDWORD)(lpMemModule + dwFirstThunk))[j]))
		{
			// ��� �� ����
			LPCSTR lpProcName = realIAT & IMAGE_ORDINAL_FLAG64 ? (LPCSTR)(realIAT & 65535) : (LPCSTR)(lpMemModule + realIAT + 2);
			//FARPROC ProcAddr = GetProcAddress(moudlebase, lpProcName);
			PVOID ProcAddr = (PVOID)MyGetProcAddress(moudlebase, lpProcName, pid);
			printf("lpProcName : %s ProcAddr : %p\r\n", lpProcName, ProcAddr);

			// ��Ч �ѵ�ַд��FirstThunk
			((FARPROC*)(lpMemModule + lpImportDescriptor[i].FirstThunk))[j] = (FARPROC)ProcAddr;

			j++;
		}

		i++;
	}

	return TRUE;
}


//��ȡӳ���С
DWORD GetImageSize(PUCHAR fileBuffer) {
	// PEͷ
	PIMAGE_DOS_HEADER lpDosHeader = (PIMAGE_DOS_HEADER)fileBuffer;
	PIMAGE_NT_HEADERS lpNtHeader = (PIMAGE_NT_HEADERS)(fileBuffer + lpDosHeader->e_lfanew);
	DWORD dwSizeOfImage = lpNtHeader->OptionalHeader.SizeOfImage;

	return dwSizeOfImage;
}

/*���PEͷ*/
VOID CleanPeHeader(PUCHAR base) {
	PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)base;
	PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS)(dos->e_lfanew + base);
	RtlZeroMemory(base, nt->OptionalHeader.SizeOfHeaders);
	DbgPrint("���PEͷ!");
}

//Զ��PE����
bool PELoaderDLL(PUCHAR fileBuffer, PUCHAR virtualBase, PVOID* PEBuffer, PULONG64 size, PVOID* entrypoint,HANDLE pid) {
	// PEͷ
	PIMAGE_DOS_HEADER lpDosHeader = (PIMAGE_DOS_HEADER)fileBuffer;
	PIMAGE_NT_HEADERS lpNtHeader = (PIMAGE_NT_HEADERS)(fileBuffer + lpDosHeader->e_lfanew);

	// ����ӳ���С
	WORD wOptionalHeaderOffset = lpNtHeader->FileHeader.SizeOfOptionalHeader - sizeof(IMAGE_OPTIONAL_HEADER);
	PIMAGE_SECTION_HEADER lpSectionHeader = (PIMAGE_SECTION_HEADER)(fileBuffer + lpDosHeader->e_lfanew + sizeof(IMAGE_NT_HEADERS) + wOptionalHeaderOffset);
	DWORD dwSizeOfImage = lpNtHeader->OptionalHeader.SizeOfImage;

	// ȡ���ֵ
	for (WORD i = 0; i < lpNtHeader->FileHeader.NumberOfSections; i++) {
		dwSizeOfImage = MAX(dwSizeOfImage, AlignedSize(lpSectionHeader[i].VirtualAddress + MAX(lpSectionHeader[i].SizeOfRawData, lpSectionHeader[i].Misc.VirtualSize), lpNtHeader->OptionalHeader.SectionAlignment));
	}

	// �����쳣
	if (0 == dwSizeOfImage) {
		return NULL;
	}

	//�����ڴ�
	ULONG_PTR lpMemModule = (ULONG_PTR)RemoteAllocateMemory(dwSizeOfImage);
	printf("lpMemModule : %p " CRLF, lpMemModule);
	if (NULL == (PVOID)lpMemModule) {
		return NULL;
	}

	// ��������
	memcpy((PVOID)lpMemModule, fileBuffer, lpDosHeader->e_lfanew + sizeof(IMAGE_NT_HEADERS) + wOptionalHeaderOffset + lpNtHeader->FileHeader.NumberOfSections * sizeof(IMAGE_SECTION_HEADER));
	for (WORD i = 0; i < lpNtHeader->FileHeader.NumberOfSections; i++)
	{
		if (0 != lpSectionHeader[i].SizeOfRawData && 0 != lpSectionHeader[i].VirtualAddress) {
			memcpy((PVOID)(lpMemModule + lpSectionHeader[i].VirtualAddress), fileBuffer + lpSectionHeader[i].PointerToRawData, lpSectionHeader[i].SizeOfRawData);
		}
	}

	// �ض����ַ
	if (FALSE == DoRelocation(lpMemModule, virtualBase))
	{
		return false;
	}

	// ��䵼���
	if (FALSE == FillRavAddress(lpMemModule, virtualBase, pid))
	{
		return false;
	}

	//���PEͷ
	CleanPeHeader((PUCHAR)lpMemModule);

	*PEBuffer = (PVOID)lpMemModule;
	*size = dwSizeOfImage;

	PVOID dllmain = (PVOID)(virtualBase + lpNtHeader->OptionalHeader.AddressOfEntryPoint);
	*entrypoint = dllmain;

	return true;
}

