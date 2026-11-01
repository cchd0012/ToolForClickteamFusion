@echo off
setlocal enabledelayedexpansion

IF "%1"=="" (
    @echo exe 파일이나 dat/ccn 파일을 Helper 파일에 드래그 앤 드랍해주세요.
) ELSE (
    :TOP
    cls

    echo ==============================
    echo ToolForClickteamFusion Helper
    echo 원하시는 기능을 입력해주세요.
    echo ext 데이터 추출
    echo rep 데이터 교체
    echo list 데이터 목록 확인
    echo locale 한국어로 로케일 변경
    echo exit 종료                           
    echo ==============================
    
    set cmdLine=ToolForClickteamFusion
    
    set /p choice=입력 기능 : 
    if "%choice%"=="ext" goto TOOL_CMD_EXTRACT
    if "%choice%"=="rep" goto TOOL_CMD_REPLACE
    if "%choice%"=="list" goto TOOL_CMD_PRINT_DATA
    if "%choice%"=="locale" goto TOOL_CMD_CHANGE_LOCALE
    if "%choice%"=="exit" goto END
    echo 잘못된 입력입니다. 다시 입력해 주세요.
    goto TOP
    
    :TOOL_CMD_EXTRACT
    cls
    echo ==============================
    echo 추출 대상 지정 복수 데이터 추가 가능
    echo 전체 지정시 곧바로 모든 데이터를 추출합니다.
    echo 1 전체
    echo 2 Image
    echo 3 Frame
    echo 4 Old Object
    echo 5 New Object
    echo 6 Font
    echo 7 Sound
    echo 8 Module
    echo 9 기타 데이터
    echo 10 데이터 추출 수행
    echo 11 취소
    echo ==============================
	
    set cmdLine=!cmdLine! -x
    
    set /p choice_extract=입력 기능 : 
    if "%choice_extract%"=="1" goto TOOL_CMD_EXTRACT_TOTAL
    if "%choice_extract%"=="2" goto TOOL_CMD_EXTRACT_IMAGE
    if "%choice_extract%"=="3" goto TOOL_CMD_EXTRACT_FRAME
    if "%choice_extract%"=="4" goto TOOL_CMD_EXTRACT_OLD_OBJECT
    if "%choice_extract%"=="5" goto TOOL_CMD_EXTRACT_NEW_OBJECT
    if "%choice_extract%"=="6" goto TOOL_CMD_EXTRACT_FONT
    if "%choice_extract%"=="7" goto TOOL_CMD_EXTRACT_SOUND
    if "%choice_extract%"=="8" goto TOOL_CMD_EXTRACT_MODULE
    if "%choice_extract%"=="9" goto TOOL_CMD_EXTRACT_OTHER
    if "%choice_extract%"=="10" goto TOOL_CMD_EXTRACT_EXECUTE
    if "%choice_extract%"=="11" goto TOP
    echo 잘못된 입력입니다. 다시 입력해 주세요.
    goto TOOL_CMD_EXTRACT
    
    :TOOL_CMD_EXTRACT_TOTAL
    set cmdLine=ToolForClickteamFusion -xt
    goto TOOL_CMD_EXTRACT_EXECUTE

    :TOOL_CMD_EXTRACT_IMAGE
    set cmdLine=!cmdLine! -img
    cls
    goto TOOL_CMD_EXTRACT

    :TOOL_CMD_EXTRACT_FRAME
    set cmdLine=!cmdLine! -frame
    cls
    goto TOOL_CMD_EXTRACT

    :TOOL_CMD_EXTRACT_OLD_OBJECT
    set cmdLine=!cmdLine! -old_obj
    cls
    goto TOOL_CMD_EXTRACT

    :TOOL_CMD_EXTRACT_NEW_OBJECT
    set cmdLine=!cmdLine! -new_obj
    cls
    goto TOOL_CMD_EXTRACT

    :TOOL_CMD_EXTRACT_FONT
    set cmdLine=!cmdLine! -font
    cls
    goto TOOL_CMD_EXTRACT

    :TOOL_CMD_EXTRACT_SOUND
    set cmdLine=!cmdLine! -sound
    cls
    goto TOOL_CMD_EXTRACT

    :TOOL_CMD_EXTRACT_MODULE
    set cmdLine=!cmdLine! -module
    cls
    goto TOOL_CMD_EXTRACT

    :TOOL_CMD_EXTRACT_OTHER
    set cmdLine=!cmdLine! -other
    cls
    goto TOOL_CMD_EXTRACT

    :TOOL_CMD_EXTRACT_EXECUTE
    set cmdLine=!cmdLine! %1
    echo %cmdLine%
    pause
    goto TOP

    :TOOL_CMD_REPLACE
    cls
    echo 데이터 교체 기능은 아직 구현되지 않았습니다.
    pause
    goto TOP

    :TOOL_CMD_PRINT_DATA
    cls
    echo 데이터 목록 확인 기능은 아직 구현되지 않았습니다.
    pause
    goto TOP

    :TOOL_CMD_CHANGE_LOCALE
    cls
    echo 한국어로 로케일 변경 기능은 아직 구현되지 않았습니다.
    pause
    goto TOP

    :END
    echo ToolForClickteamFusion Helper를 종료합니다.
)

pause
