#undef UNICODE   
#undef _UNICODE   
#include <windows.h> 

#include <tchar.h>   
#include <stdio.h> 

#include <pdh.h>   

#pragma comment(lib, "pdh.lib") 


int main() {
    HQUERY hQuery = NULL;
    HCOUNTER hCounter = NULL;

    PDH_STATUS pdhStatus;
    PDH_FMT_COUNTERVALUE DisplayValue;          //计数器值  
    DWORD dwCounterType = 0;                    //计数器类型  

    // open query   
    pdhStatus = PdhOpenQuery(NULL, 0, &hQuery);
    if (pdhStatus != ERROR_SUCCESS) {
        printf("PdhOpenQuery failed/n");
        exit(1);
    }

    // construct a counter path   
    PDH_COUNTER_PATH_ELEMENTS pcpe;
    TCHAR szFullPathBuffer[MAX_PATH] = TEXT("");
    DWORD dwSize = sizeof(szFullPathBuffer);

    pcpe.szMachineName = NULL;
    pcpe.szObjectName = (LPSTR)"Processor";
    pcpe.szInstanceName = (LPSTR)"_Total";
    pcpe.szCounterName = (LPSTR)"% Processor Time";
    pcpe.dwInstanceIndex = -1;
    pcpe.szParentInstance = NULL;

    pdhStatus = PdhMakeCounterPath(&pcpe, szFullPathBuffer, &dwSize, 0);
    if (pdhStatus != ERROR_SUCCESS) {
        printf("PdhMakeCounterPath failed/n");
        goto exit_prog;
    }
    _tprintf(TEXT("path: %s/n"), szFullPathBuffer);

    // add a counter    
    pdhStatus = PdhAddCounter(hQuery, (LPCSTR)"\\Processor(_Total)\\% Processor Time", 0, &hCounter);
    //pdhStatus = PdhAddCounter(hQuery, TEXT("//Processor(_Total)//% Processor Time"), 0, &hCounter);

    if (pdhStatus != ERROR_SUCCESS) {
        printf("PdhAddCounter failed/n");
        goto exit_prog;
    }

    // collect query data   
    pdhStatus = PdhCollectQueryData(hQuery);
    //pdhStatus = PdhCollectQueryDataEx(hQuery, 1, NULL);

    if (pdhStatus != ERROR_SUCCESS) {
        printf("PdhCollectQueryData failed/n");
        goto exit_prog;
    }
    while (true)
    {
        pdhStatus = PdhCollectQueryData(hQuery);
        pdhStatus = PdhGetFormattedCounterValue(hCounter,
            PDH_FMT_DOUBLE,
            &dwCounterType, &DisplayValue);
        printf("CPU占用 %.2f\n", DisplayValue.doubleValue);
        Sleep(1000);                    //1秒收集1次  
    }

exit_prog:
    PdhCloseQuery(hQuery);
    return 0;

    //PDH_STATUS status = ERROR_SUCCESS;
    //HQUERY hQuery = NULL;                       //计数器对象句柄  
    //HCOUNTER hCounter = NULL;                   //计数器句柄  
    //PDH_FMT_COUNTERVALUE DisplayValue;          //计数器值  
    //DWORD dwCounterType = 0;                    //计数器类型  
    //status = PdhOpenQuery(NULL, 0, &hQuery);    //打开计数器对象  
    //if (ERROR_SUCCESS == status)
    //{
    //    //PdhAddCounter()函数添加计数器，参数中Processor代表计数器对象，(0)表示          第1个处理器计数实例。如果计算机上有两个处理器，则第2个处理器计数实例用（1）     表示  
    //    //% Processor Time表示计数器（取得处理器占用时间）,注意%后面有个空格  
    //    status = PdhAddCounter(hQuery, (LPCSTR)"\\Processor(_Total)\\% Processor Time", 0, &hCounter);
    //    if (ERROR_SUCCESS == status)
    //    {
    //        // PdhCollectQueryData()函数收集数据需两次调用, PdhCollectQuery  DataEx只需一次  
    //        PdhCollectQueryData(hQuery);        //首次收集数据  
    //        while (true)
    //        {
    //            PdhCollectQueryData(hQuery);    //再次收集数据  
    //            status = PdhGetFormattedCounterValue(hCounter,
    //                PDH_FMT_DOUBLE,
    //                &dwCounterType, &DisplayValue);
    //            //取得收集的数据值  
    //            printf("CPU占用 %.2f\n", DisplayValue.doubleValue);
    //            Sleep(1000);                    //1秒收集1次  
    //        }
    //    }
    //    PdhCloseQuery(hQuery);                  //关闭计数器对象  
    //}
}

