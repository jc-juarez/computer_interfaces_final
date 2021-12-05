// Computer Interfaces
// HID Device Lookup and Control
// Juan Carlos Juárez
// Jesus Eduardo García

#include <stdio.h>
#include <conio.h>
#include <stdlib.h>
#include <windows.h>
#include <SETUPAPI.H>

//----------------------------------------------
int RICH_VENDOR_ID;
int RICH_USBHID_GENIO_ID;

#define INPUT_REPORT_SIZE	64
#define OUTPUT_REPORT_SIZE	64
//----------------------------------------------

typedef struct _HIDD_ATTRIBUTES {
	ULONG   Size; // = sizeof (struct _HIDD_ATTRIBUTES)
	USHORT  VendorID;
	USHORT  ProductID;
	USHORT  VersionNumber;
} HIDD_ATTRIBUTES, * PHIDD_ATTRIBUTES;

typedef VOID(__stdcall* PHidD_GetProductString)(HANDLE, PVOID, ULONG);
typedef VOID(__stdcall* PHidD_GetHidGuid)(LPGUID);
typedef BOOLEAN(__stdcall* PHidD_GetAttributes)(HANDLE, PHIDD_ATTRIBUTES);
typedef BOOLEAN(__stdcall* PHidD_SetFeature)(HANDLE, PVOID, ULONG);
typedef BOOLEAN(__stdcall* PHidD_GetFeature)(HANDLE, PVOID, ULONG);

//----------------------------------------------

HINSTANCE                       hHID = NULL;
PHidD_GetProductString          HidD_GetProductString = NULL;
PHidD_GetHidGuid                HidD_GetHidGuid = NULL;
PHidD_GetAttributes             HidD_GetAttributes = NULL;
PHidD_SetFeature                HidD_SetFeature = NULL;
PHidD_GetFeature                HidD_GetFeature = NULL;
HANDLE                          DeviceHandle = INVALID_HANDLE_VALUE;

unsigned int moreHIDDevices = TRUE;
unsigned int HIDDeviceFound = FALSE;

unsigned int terminaAbruptaEInstantaneamenteElPrograma = 0;

void Load_HID_Library(void) {
	hHID = LoadLibrary("HID.DLL");
	if (!hHID) {
		printf("Failed to load HID.DLL\n");
		return;
	}

	HidD_GetProductString = (PHidD_GetProductString)GetProcAddress(hHID, "HidD_GetProductString");
	HidD_GetHidGuid = (PHidD_GetHidGuid)GetProcAddress(hHID, "HidD_GetHidGuid");
	HidD_GetAttributes = (PHidD_GetAttributes)GetProcAddress(hHID, "HidD_GetAttributes");
	HidD_SetFeature = (PHidD_SetFeature)GetProcAddress(hHID, "HidD_SetFeature");
	HidD_GetFeature = (PHidD_GetFeature)GetProcAddress(hHID, "HidD_GetFeature");

	if (!HidD_GetProductString
		|| !HidD_GetAttributes
		|| !HidD_GetHidGuid
		|| !HidD_SetFeature
		|| !HidD_GetFeature) {
		printf("Couldn't find one or more HID entry points\n");
		return;
	}
}

int Open_Device(void) {
	HDEVINFO							DeviceInfoSet;
	GUID								InterfaceClassGuid;
	SP_DEVICE_INTERFACE_DATA			DeviceInterfaceData;
	PSP_DEVICE_INTERFACE_DETAIL_DATA	pDeviceInterfaceDetailData;
	HIDD_ATTRIBUTES						Attributes;
	DWORD								Result;
	DWORD								MemberIndex = 0;
	DWORD								Required;

	//Validar si se "cargó" la biblioteca (DLL)
	if (!hHID)
		return (0);

	//Obtener el Globally Unique Identifier (GUID) para dispositivos HID
	HidD_GetHidGuid(&InterfaceClassGuid);
	//Sacarle a Windows la información sobre todos los dispositivos HID instalados y activos en el sistema
	// ... almacenar esta información en una estructura de datos de tipo HDEVINFO
	DeviceInfoSet = SetupDiGetClassDevs(&InterfaceClassGuid, NULL, NULL, DIGCF_PRESENT | DIGCF_INTERFACEDEVICE);
	if (DeviceInfoSet == INVALID_HANDLE_VALUE)
		return (0);

	//Obtener la interfaz de comunicación con cada uno de los dispositivos para preguntarles información específica
	DeviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
	while (!HIDDeviceFound) {
		// ... utilizando la variable MemberIndex ir preguntando dispositivo por dispositivo ...
		moreHIDDevices = SetupDiEnumDeviceInterfaces(DeviceInfoSet, NULL, &InterfaceClassGuid,
			MemberIndex, &DeviceInterfaceData);
		if (!moreHIDDevices) {
			// ... si llegamos al fin de la lista y no encontramos al dispositivo ==> terminar y marcar error
			SetupDiDestroyDeviceInfoList(DeviceInfoSet);
			return (0); //No more devices found
		}
		else {
			//Necesitamos preguntar, a través de la interfaz, el PATH del dispositivo, para eso ...
			// ... primero vamos a ver cuántos caracteres se requieren (Required)
			Result = SetupDiGetDeviceInterfaceDetail(DeviceInfoSet, &DeviceInterfaceData, NULL, 0, &Required, NULL);
			pDeviceInterfaceDetailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA)malloc(Required);
			if (pDeviceInterfaceDetailData == NULL) {
				printf("Error en SetupDiGetDeviceInterfaceDetail\n");
				return (0);
			}
			//Ahora si, ya que el "buffer" fue preparado (pDeviceInterfaceDetailData{DevicePath}), vamos a preguntar PATH
			pDeviceInterfaceDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
			Result = SetupDiGetDeviceInterfaceDetail(DeviceInfoSet, &DeviceInterfaceData, pDeviceInterfaceDetailData,
				Required, NULL, NULL);
			if (!Result) {
				printf("Error en SetupDiGetDeviceInterfaceDetail\n");
				free(pDeviceInterfaceDetailData);
				return(0);
			}
			//Para este momento ya sabemos el PATH del dispositivo, ahora hay que preguntarle ...
			// ... su VID y PID, para ver si es con quien nos interesa comunicarnos
			printf("Found? ==> ");
			printf("Device: %s\n", pDeviceInterfaceDetailData->DevicePath);

			//Obtener un "handle" al dispositivo
			DeviceHandle = CreateFile(pDeviceInterfaceDetailData->DevicePath,
				GENERIC_READ | GENERIC_WRITE,
				FILE_SHARE_READ | FILE_SHARE_WRITE,
				(LPSECURITY_ATTRIBUTES)NULL,
				OPEN_EXISTING,
				0,
				NULL);

			if (DeviceHandle == INVALID_HANDLE_VALUE) {
				printf("¡¡¡Error en el CreateFile!!!\n");
			}
			else {
				//Preguntar por los atributos del dispositivo
				Attributes.Size = sizeof(Attributes);
				Result = HidD_GetAttributes(DeviceHandle, &Attributes);
				if (!Result) {
					printf("Error en HIdD_GetAttributes\n");
					CloseHandle(DeviceHandle);
					free(pDeviceInterfaceDetailData);
					return(0);
				}
				//Analizar los atributos del dispositivo para verificar el VID y PID
				printf("MemberIndex=%d,VID=%04x,PID=%04x\n", MemberIndex, Attributes.VendorID, Attributes.ProductID);
				if ((Attributes.VendorID == RICH_VENDOR_ID) && (Attributes.ProductID == RICH_USBHID_GENIO_ID)) {
					printf("USB/HID GenIO ==> ");
					printf("Device: %s\n", pDeviceInterfaceDetailData->DevicePath);
					HIDDeviceFound = TRUE;
				}
				else
					CloseHandle(DeviceHandle);

			}
			MemberIndex++;
			free(pDeviceInterfaceDetailData);
			if (HIDDeviceFound) {
				printf("\nSpecified HID Located. Starting System.\n");
				getchar();
			}
		}
	}
	return(1);
}

void Close_Device(void) {
	if (DeviceHandle != NULL) {
		CloseHandle(DeviceHandle);
		DeviceHandle = NULL;
	}
}

void Touch_Device(char option) {
	DWORD BytesRead = 0;
	DWORD BytesWritten = 0;
	unsigned char reporteEntrada[INPUT_REPORT_SIZE + 1];
	unsigned char reporteSalida[OUTPUT_REPORT_SIZE + 1];
	int status = 0;
	int ledStatus = 0;
	static unsigned char dato = 0x01;
	reporteSalida[0] = 0x00; 
	printf("\n");
	if (DeviceHandle == NULL)	return;
	if (option == '1' || option == '2' || option == '3') {
		printf("Select Turn On (1) // Turn Off (0): ");
		scanf_s("%d", &ledStatus);
		reporteSalida[2] = ledStatus;
	}
	switch (option) {
		case '1':
			reporteSalida[1] = 0x01;
			status = WriteFile(DeviceHandle, reporteSalida, OUTPUT_REPORT_SIZE + 1, &BytesWritten, NULL);
			if (!status) printf("An Error Has Ocurred");
			break;
		case '2':
			reporteSalida[1] = 0x02;
			status = WriteFile(DeviceHandle, reporteSalida, OUTPUT_REPORT_SIZE + 1, &BytesWritten, NULL);
			if (!status) printf("An Error Has Ocurred");
			break;
		case '3':
			reporteSalida[1] = 0x03;
			status = WriteFile(DeviceHandle, reporteSalida, OUTPUT_REPORT_SIZE + 1, &BytesWritten, NULL);
			if (!status) printf("An Error Has Ocurred");
			break;
		case '4':
			reporteSalida[1] = 0x81;
			reporteSalida[2] = 0;
			status = WriteFile(DeviceHandle, reporteSalida, OUTPUT_REPORT_SIZE + 1, &BytesWritten, NULL);
			if (!status) printf("An Error Has Ocurred");
			status = ReadFile(DeviceHandle, reporteEntrada, INPUT_REPORT_SIZE + 1, &BytesRead, NULL);
			if (!status) printf("An Error Has Ocurred");
			if (!reporteEntrada[2]) {
				printf("No Switches Pressed.\n");
			}
			else {
				int curr = reporteEntrada[2];
				if (curr && curr % 2 != 0) {
					printf("Switch 1 is On");
				}
				else {
					printf("Switch 1 is Off");
				}
				if (curr) curr >>= 1;
				if (curr && curr % 2 != 0) {
					printf(" | Switch 2 is On");
				}
				else {
					printf(" | Switch 2 is Off");
				}
				if (curr) curr >>= 1;
				if (curr && curr % 2 != 0) {
					printf(" | Switch 3 is On.\n");
				}
				else {
					printf(" | Switch 3 is Off.\n");
				}
			}
			break;
		case '5':
			reporteSalida[1] = 0x82;
			reporteSalida[2] = 0;
			status = WriteFile(DeviceHandle, reporteSalida, OUTPUT_REPORT_SIZE + 1, &BytesWritten, NULL);
			if (!status) printf("An Error Has Ocurred");
			status = ReadFile(DeviceHandle, reporteEntrada, INPUT_REPORT_SIZE + 1, &BytesRead, NULL);
			if (!status) printf("An Error Has Ocurred");
			printf("Students IDs: ");
			for (int i = 2; reporteEntrada[i] != 0 && i < 64; i++)
				printf("%c", reporteEntrada[i]);
			printf("\n");
			break;
		default:
			printf("\nSomething went wrong\n");
			break;
	};
}

void main() {
	char option = ' ';
	printf("* Final Project - Computer Interfaces *\n");
	printf("Juan Carlos Juarez A00824524 | Jesus Eduardo Garcia A00825235.\n\n");
	printf("Please Insert the Vendor ID: ");
	scanf_s("%x", &RICH_VENDOR_ID);
	printf("Please Insert the Product ID: ");
	scanf_s("%x", &RICH_USBHID_GENIO_ID);
	printf("\n");
	printf("Starting HID Device Search.\n\n");
	Load_HID_Library();
	if (Open_Device()) {
		do {
			printf("\nPlease Select an Option: \n\n");
			printf("1) Modify LED 1.\n");
			printf("2) Modify LED 2.\n");
			printf("3) Modify LED 3.\n");
			printf("4) Read Buttons Current Status.\n");
			printf("5) Read Students IDs.\n");
			printf("e) Exit Program.\n");
			printf("Your Option: ");
			scanf_s(" %c", &option);
			if (option == '1' || option == '2' || option == '3' || option == '4' || option == '5') Touch_Device(option);
			else if (option != 'e') printf("\nPlease select a valid Option.\n");
		} while (option != 'e');
	}
	else {
		printf("\nNo HID with the required specs found.");
	}
	printf("\nExecution Finished<>\n");
	Close_Device();
}