/****************************************************************************
 *
 * CRI Middleware SDK
 *
 * Copyright (c) 2006-2017 CRI Middleware Co., Ltd.
 *
 * Library  : CRI File System
 * Module   : Library User's Header
 * File     : cri_file_system.h
 *
 ****************************************************************************/
/*!
 *	\file		cri_file_system.h
 */

/* 多重定義防止					*/
/* Prevention of redefinition	*/
#ifndef	CRI_FILE_SYSTEM_H_INCLUDED
#define	CRI_FILE_SYSTEM_H_INCLUDED

/***************************************************************************
 *      インクルードファイル
 *      Include files
 ***************************************************************************/
#include "cri_le_xpt.h"
#include "cri_le_error.h"

#ifdef __cplusplus
#if defined(XPT_TGT_TBCG)
#if !defined(XPT_DISABLE_FSV1API)
	/* 旧バージョンAPI */
	/* Old version API */
	#include "cri_file_system_ver1api.h"
#endif
#endif
#endif

/***************************************************************************
 *      定数マクロ
 *      Macro Constants
 ***************************************************************************/
/* バージョン情報 */
/* Version Number */
#define CRI_FS_VER_NAME		"CRI File System (LE)"
#define CRI_FS_VER_NUM		"2.85.1"
#define CRI_FS_VER_MAJOR	(2)
#define CRI_FS_VER_MINOR	(85)
#define CRI_FS_VER_PATCH	(1)
#define CRI_FS_VER_RELEASE	(0)
#define CRI_FS_VER_REVISION	(0)

/*JP
 * \brief コンフィギュレーションのデフォルト値
 */
#if defined(XPT_TGT_NITRO)
#define	CRIFS_CONFIG_DEFAULT_THREAD_MODEL		CRIFS_THREAD_MODEL_MULTI
#define	CRIFS_CONFIG_DEFAULT_NUM_BINDERS		(8)
#define	CRIFS_CONFIG_DEFAULT_NUM_LOADERS		(32)
#define	CRIFS_CONFIG_DEFAULT_NUM_GROUP_LOADERS	(2)
#define	CRIFS_CONFIG_DEFAULT_NUM_STDIO_HANDLES	(4)
#define	CRIFS_CONFIG_DEFAULT_NUM_INSTALLERS		(0)
#define	CRIFS_CONFIG_DEFAULT_MAX_BINDS			(8)
#define	CRIFS_CONFIG_DEFAULT_MAX_FILES			(32)
#define	CRIFS_CONFIG_DEFAULT_MAX_PATH			(128)
#elif defined(XPT_TGT_TBCG)
#define	CRIFS_CONFIG_DEFAULT_THREAD_MODEL		CRIFS_THREAD_MODEL_MULTI
#define	CRIFS_CONFIG_DEFAULT_NUM_BINDERS		(8)
#define	CRIFS_CONFIG_DEFAULT_NUM_LOADERS		(32)
#define	CRIFS_CONFIG_DEFAULT_NUM_GROUP_LOADERS	(16)
#define	CRIFS_CONFIG_DEFAULT_NUM_STDIO_HANDLES	(1)
#define	CRIFS_CONFIG_DEFAULT_NUM_INSTALLERS		(0)
#define	CRIFS_CONFIG_DEFAULT_MAX_BINDS			(16)
#define	CRIFS_CONFIG_DEFAULT_MAX_FILES			(32)
#define	CRIFS_CONFIG_DEFAULT_MAX_PATH			(256)
#elif defined(XPT_TGT_EMSCRIPTEN)
#define	CRIFS_CONFIG_DEFAULT_THREAD_MODEL		CRIFS_THREAD_MODEL_SINGLE
#define	CRIFS_CONFIG_DEFAULT_NUM_BINDERS		(8)
#define	CRIFS_CONFIG_DEFAULT_NUM_LOADERS		(32)
#define	CRIFS_CONFIG_DEFAULT_NUM_GROUP_LOADERS	(16)
#define	CRIFS_CONFIG_DEFAULT_NUM_STDIO_HANDLES	(1)
#define	CRIFS_CONFIG_DEFAULT_NUM_INSTALLERS		(0)
#define	CRIFS_CONFIG_DEFAULT_MAX_BINDS			(16)
#define	CRIFS_CONFIG_DEFAULT_MAX_FILES			(32)
#define	CRIFS_CONFIG_DEFAULT_MAX_PATH			(256)
#elif defined(CRI_USE_FILESYSTEM_PLATFORM_HEADER)
#include "cri_file_system_platform.h"
#else
#define	CRIFS_CONFIG_DEFAULT_THREAD_MODEL		CRIFS_THREAD_MODEL_MULTI
#define	CRIFS_CONFIG_DEFAULT_NUM_BINDERS		(16)
#define	CRIFS_CONFIG_DEFAULT_NUM_LOADERS		(16)
#define	CRIFS_CONFIG_DEFAULT_NUM_GROUP_LOADERS	(2)
#define	CRIFS_CONFIG_DEFAULT_NUM_STDIO_HANDLES	(16)
#define	CRIFS_CONFIG_DEFAULT_NUM_INSTALLERS		(0)
#define	CRIFS_CONFIG_DEFAULT_MAX_BINDS			(16)
#define	CRIFS_CONFIG_DEFAULT_MAX_FILES			(16)
#define	CRIFS_CONFIG_DEFAULT_MAX_PATH			(256)
#endif

/*JP
 * \brief デフォルトデバイスID
 */
#define CRIFS_DEVICE_DEFAULT	(CRIFS_DEVICE_00)
/*JP
 * \brief メモリファイルシステムデバイスID
 */
#define CRIFS_DEVICE_MEMORY		(CRIFS_DEVICE_07)

/*JP
 * \brief デフォルトデバイスID旧定義
 * \par 備考:
 * この定義は廃止予定です。かわりに::CRIFS_DEVICE_DEFAULTを使用してください。
 * \sa CRIFS_DEVICE_DEFAULT
 */
#define CRIFS_DEFAULT_DEVICE	(CRIFS_DEVICE_DEFAULT)

/*JP
 * \brief メモリファイルパスの最大長
 * \ingroup FSLIB_CRIFS
 * \par 説明:
 * メモリファイルパスの最大長です。NULL文字分を含みます。<br>
 * メモリファイルパス長がこの数値を超過することはありません。<br>
 * \attention
 * この値は、将来大きくなる可能性があります。<br>
 * \sa criFs_AddressToPath
 */
#define CRIFS_MAX_MEMORY_FILE_PATH	(44)

/*JP
 * \brief 無効なバインドID
 * \ingroup FSLIB_BINDER
 * \par 説明:
 * 未使用バインダーに与えられるIDです。バインドに失敗した時にも返ってくることがあります。<br>
 * \sa CriFsBindId, criFsBinder_BindCpk, criFsBinder_BindFile, criFsBinder_BindFiles, criFsBinder_BindDirectory
 */
#define CRIFSBINDER_BID_NULL		(0)

/*JP
 * \brief 有効バインドIDの開始番号
 * \ingroup FSLIB_BINDER
 * \par 説明:
 * 有効なバインドIDの開始番号です。<br>
 * 各種バインド関数で返すIDがこのIDよりも大きければ有効です。<br>
 * \sa CriFsBindId, criFsBinder_BindCpk, criFsBinder_BindFile, criFsBinder_BindFiles, criFsBinder_BindDirectory
 */
#define CRIFSBINDER_BID_START		(1)

/***************************************************************************
 *      処理マクロ
 *      Macro Functions
 ***************************************************************************/
/*JP
 * \brief ユーザーアロケーターの登録
 * \ingroup FSLIB_CRIFS
 * \param[in]	p_malloc_func	メモリ確保関数
 * \param[in]	p_free_func		メモリ解放関数
 * \param[in]	p_obj			ユーザー指定オブジェクト
 * \par 説明:
 * CRI File Systemライブラリにメモリアロケーター（メモリの確保／解放関数）を登録します。<br>
 * CRI File Systemライブラリ内がライブラリ内で行なうメモリ解放処理を、
 * ユーザー独自のメモリ解放処理に置き換えたい場合に使用します。
 */
#define criFs_SetUserAllocator(p_malloc_func, p_free_func, p_obj)	\
{\
	criFs_SetUserMallocFunction(p_malloc_func, p_obj);\
	criFs_SetUserFreeFunction(p_free_func, p_obj);\
}

/***************************************************************************
 *      データ型宣言
 *      Data Type Declarations
 ***************************************************************************/
/*==========================================================================
 *      CRI File System API
 *=========================================================================*/
/*JP
 * \brief スレッドモデル
 * \par 説明:
 * CRI File Systemライブラリがどのようなスレッドモデルで動作するかを表します。<br>
 * ライブラリ初期化時（::criFs_InitializeLibrary 関数）に、::CriFsConfig 構造体にて指定します。
 * \sa CriFsConfig
 * \sa criFs_InitializeLibrary
 */
typedef enum CriFsThreadModelTag {
	/*JP
	 * \brief マルチスレッド
	 * \par 説明:
	 * ライブラリは内部でスレッドを作成し、マルチスレッドにて動作します。<br>
	 * スレッドは ::criFs_InitializeLibrary 関数呼び出し時に作成されます。
	 */
	/*EN
	 * \brief Multithread
	 * \par Description:
	 * The library creates threads inside and operates in multithread environment. <br>
	 * A thread is created when the ::criFs_InitializeLibrary function is called.
	 */
	CRIFS_THREAD_MODEL_MULTI = 0,

	/*JP
	 * \brief マルチスレッド（ユーザー駆動式）
	 * \par 説明:
	 * ライブラリは内部でスレッドを作成し、マルチスレッドにて動作します。<br>
	 * スレッドは ::criFs_InitializeLibrary 関数呼び出し時に作成されます。<br>
	 * サーバー処理自体は作成されたスレッド上で実行されますが、
	 * CRIFS_THREAD_MODEL_MULTI とは異なり、自動的には実行されません。<br>
	 * ユーザーは ::criFs_ExecuteMain 関数で明示的にサーバー処理を駆動する必要があります。<br>
	 * （  ::criFs_ExecuteMain 関数を実行すると、スレッドが起動し、サーバー処理が実行されます。）<br>
	 */
	CRIFS_THREAD_MODEL_MULTI_USER_DRIVEN = 3,

	/*JP
	 * \brief ユーザーマルチスレッド
	 * \par 説明:
	 * ライブラリ内部ではスレッドを作成しませんが、ユーザーが独自に作成したスレッドからサーバー処理関数（::criFs_ExecuteFileAccess 関数、::criFs_ExecuteDataDecompression 関数）を呼び出せるよう、内部の排他制御は行います。
	 */
	/*EN
	 * \brief User multithread
	 * \par Description:
	 * No thread is created but exclusion control is performed inside the library for the server processing functions (::criFs_ExecuteFileAccess, ::criFs_ExecuteDataDecompression) to be able to be called from a user-created thread.
	 */
	CRIFS_THREAD_MODEL_USER_MULTI = 1,

	/*JP
	 * \brief シングルスレッド
	 * \par 説明:
	 * ライブラリ内部でスレッドを作成しません。また、内部の排他制御も行いません。<br>
	 * このモデルを選択した場合、各APIとサーバー処理関数（::criFs_ExecuteFileAccess 関数、::criFs_ExecuteDataDecompression 関数）とを同一スレッドから呼び出すようにしてください。
	 */
	/*EN
	 * \brief Single thread
	 * \par Description:
	 * No thread is created inside the library. Exclusion control is not performed inside the library either. <br>
	 * When selecting this model, call the APIs and server processing functions (::criFs_ExecuteFileAccess, ::criFs_ExecuteDataDecompression) from the same thread.
	 */
	CRIFS_THREAD_MODEL_SINGLE = 2,

	/* enum be 4bytes */
	CRIFS_THREAD_MODEL_ENUM_BE_SINT32 = 0x7FFFFFFF
} CriFsThreadModel;

/*JP
 * \brief コンフィギュレーション
 * \par 説明:
 * CRI File Systemライブラリの動作仕様を指定するための構造体です。<br>
 * ライブラリ初期化時（ ::criFs_InitializeLibrary 関数）に引数として本構造体を指定します。<br>
 * \par
 * CRI File Systemライブラリは、初期化時に指定されたコンフィギュレーションに応じて、内部リソースを必要な数分だけ確保します。<br>
 * そのため、コンフィギュレーションに指定する値を小さくすることで、ライブラリが必要とするメモリのサイズを小さく抑えることが可能です。<br>
 * ただし、コンフィギュレーションに指定した数以上のハンドルを確保することはできなくなるため、値を小さくしすぎると、ハンドルの確保に失敗する可能性があります。<br>
 * \par 備考:
 * デフォルト設定を使用する場合、 ::criFs_SetDefaultConfig 関数でデフォルトパラメータをセットし、 ::criFs_InitializeLibrary 関数に指定してください。<br>
 * \attention
 * 将来的にメンバーが増える可能性に備え、設定前に::criFs_SetDefaultConfig 関数で初期化してから使用してください。<br>
 * \sa criFs_InitializeLibrary, criFs_SetDefaultConfig
 */
typedef struct CriFsConfigTag {
	/*JP
		\brief スレッドモデル
		\par 説明:
		CRI File Systemのスレッドモデルを指定します。<br>
		\sa CriFsThreadModel
	*/
	CriFsThreadModel thread_model;

	/*JP
		\brief 使用するCriFsBinderの数
		\par 説明:
		アプリケーション中で使用するバインダー（CriFsBinder）の数を指定します。<br>
		アプリケーション中で ::criFsBinder_Create 関数を使用してバインダーを作成する場合、
		本パラメータに使用するバインダーの数を指定する必要があります。<br>
		<br>
		num_bindersには「同時に使用するバインダーの最大数」を指定します。<br>
		例えば、 ::criFsBinder_Create 関数と ::criFsBinder_Destroy 関数を交互に続けて実行するケースにおいては、
		最大同時には1つのバインダーしか使用しないため、関数の呼び出し回数に関係なくnum_bindersに1を指定することが可能です。<br>
		逆に、ある場面でバインダーを10個使用する場合には、その他の場面でバインダーを全く使用しない場合であっても、
		num_bindersに10を指定する必要があります。<br>
		\par 備考:
		CRI File Systemライブラリは、使用するバインダーの数分だけのメモリを初期化時に要求します。<br>
		そのため、num_bindersに必要最小限の値をセットすることで、ライブラリが必要とするメモリのサイズを抑えることが可能です。<br>
		\sa criFsBinder_Create, criFsBinder_Destroy
	*/
	CriSint32 num_binders;

	/*JP
		\brief 使用するCriFsLoaderの数
		\par 説明:
		アプリケーション中で使用するローダー（CriFsLoader）の数を指定します。<br>
		アプリケーション中で ::criFsLoader_Create 関数を使用してローダーを作成する場合、
		本パラメータに使用するローダーの数を指定する必要があります。<br>
		<br>
		num_loadersには「同時に使用するローダーの最大数」を指定します。<br>
		例えば、 ::criFsLoader_Create 関数と ::criFsLoader_Destroy 関数を交互に続けて実行するケースにおいては、
		最大同時には1つのローダーしか使用しないため、関数の呼び出し回数に関係なくnum_loadersに1を指定することが可能です。<br>
		逆に、ある場面でローダーを10個使用する場合には、その他の場面でローダーを全く使用しない場合であっても、
		num_loadersに10を指定する必要があります。<br>
		\par 備考:
		CRI File Systemライブラリは、使用するローダーの数分だけのメモリを初期化時に要求します。<br>
		そのため、num_loadersに必要最小限の値をセットすることで、ライブラリが必要とするメモリのサイズを抑えることが可能です。<br>
		\sa criFsLoader_Create, criFsLoader_Destroy
	*/
	CriSint32 num_loaders;

	/*JP
		\brief 使用するCriFsGroupLoaderの数
		\par 説明:
		アプリケーション中で使用するグループローダー（CriFsGroupLoader）の数を指定します。<br>
		アプリケーション中で ::criFsGroupLoader_Create 関数を使用してグループローダーを作成する場合、
		本パラメータに使用するグループローダーの数を指定する必要があります。<br>
		<br>
		num_group_loadersには「同時に使用するグループローダーの最大数」を指定します。<br>
		例えば、 ::criFsGroupLoader_Create 関数と ::criFsGroupLoader_Destroy 関数を交互に続けて実行するケースにおいては、
		最大同時には1つのグループローダーしか使用しないため、関数の呼び出し回数に関係なくnum_group_loadersに1を指定することが可能です。<br>
		逆に、ある場面でグループローダーを10個使用する場合には、その他の場面でグループローダーを全く使用しない場合であっても、
		num_group_loadersに10を指定する必要があります。<br>
		\par 備考:
		CRI File Systemライブラリは、使用するグループローダーの数分だけのメモリを初期化時に要求します。<br>
		そのため、num_group_loadersに必要最小限の値をセットすることで、ライブラリが必要とするメモリのサイズを抑えることが可能です。<br>
		\sa criFsGroupLoader_Create, criFsGroupLoader_Destroy
	*/
	CriSint32 num_group_loaders;

	/*JP
		\brief 使用するCriFsStdioの数
		\par 説明:
		アプリケーション中で使用するCriFsStdioハンドルの数を指定します。<br>
		アプリケーション中で ::criFsStdio_OpenFile 関数を使用してCriFsStdioハンドルを作成する場合、
		本パラメータに使用するCriFsStdioハンドルの数を指定する必要があります。<br>
		<br>
		num_stdio_handlesには「同時に使用するCriFsStdioハンドルの最大数」を指定します。<br>
		例えば、 ::criFsStdio_OpenFile 関数と ::criFsStdio_CloseFile 関数を交互に続けて実行するケースにおいては、
		最大同時には1つのCriFsStdioハンドルしか使用しないため、関数の呼び出し回数に関係なくnum_stdio_handlesに1を指定することが可能です。<br>
		逆に、ある場面でCriFsStdioハンドルを10個使用する場合には、その他の場面でCriFsStdioハンドルを全く使用しない場合であっても、
		num_stdio_handlesに10を指定する必要があります。<br>
		\par 備考:
		CRI File Systemライブラリは、使用するCriFsStdioハンドルの数分だけのメモリを初期化時に要求します。<br>
		そのため、num_stdio_handlesに必要最小限の値をセットすることで、ライブラリが必要とするメモリのサイズを抑えることが可能です。<br>
		\attention
		ブリッジライブラリを使用してADXライブラリや救声主ライブラリを併用する場合、
		ADXTハンドルやcriSsPlyハンドルは内部的にCriFsStdioハンドルを作成します。<br>
		そのため、ブリッジライブラリを使用する場合には、CRI File Systemライブラリ初期化時に
		num_stdio_handlesにADXTハンドルやcriSsPlyハンドルの数を加えた値を指定してください。<br>
		\sa criFsStdio_OpenFile, criFsStdio_CloseFile
	*/
	CriSint32 num_stdio_handles;

	/*JP
		\brief 使用するCriFsInstallerの数
		\par 説明:
		アプリケーション中で使用するインストーラー（CriFsInstaller）の数を指定します。<br>
		アプリケーション中で criFsInstaller_Create 関数を使用してインストーラーを作成する場合、
		本パラメータに使用するインストーラーの数を指定する必要があります。<br>
		<br>
		num_installersには「同時に使用するインストーラーの最大数」を指定します。<br>
		例えば、 criFsInstaller_Create 関数と criFsInstaller_Destroy 関数を交互に続けて実行するケースにおいては、
		最大同時には1つのインストーラーしか使用しないため、関数の呼び出し回数に関係なくnum_installersに1を指定することが可能です。<br>
		逆に、ある場面でインストーラーを10個使用する場合には、その他の場面でインストーラーを全く使用しない場合であっても、
		num_installersに10を指定する必要があります。<br>
		\par 備考:
		CRI File Systemライブラリは、使用するインストーラーの数分だけのメモリを初期化時に要求します。<br>
		そのため、num_installersに必要最小限の値をセットすることで、ライブラリが必要とするメモリのサイズを抑えることが可能です。<br>
		\attention
		::criFs_SetDefaultConfig マクロを使用してコンフィギュレーションを初期化する場合、num_installersの数は0に設定されます。<br>
		そのため、インストーラーを使用する場合には、アプリケーション中でnum_installersを明示的に指定する必要があります。<br>
	*/
	CriSint32 num_installers;

	/*JP
		\brief 最大同時バインド数
		\par 説明:
		アプリケーション中でバインド処理を行い、保持するバインドID（CriFsBindId）の数を指定します。<br>
		アプリケーション中で ::criFsBinder_BindCpk 関数等を使用してバインド処理を行う場合、
		本パラメータに使用するバインドIDの数を指定する必要があります。<br>
		<br>
		max_bindsには「同時に使用するバインドIDの最大数」を指定します。<br>
		例えば、 ::criFsBinder_BindCpk 関数と ::criFsBinder_Unbind 関数を交互に続けて実行するケースにおいては、
		最大同時には1つのバインドIDしか使用しないため、関数の呼び出し回数に関係なくmax_bindsに1を指定することが可能です。<br>
		逆に、ある場面でバインドIDを10個使用する場合には、その他の場面でバインドを一切行わない場合であっても、
		max_bindsに10を指定する必要があります。<br>
		\par 備考:
		CRI File Systemライブラリは、使用するバインドIDの数分だけのメモリを初期化時に要求します。<br>
		そのため、max_bindsに必要最小限の値をセットすることで、ライブラリが必要とするメモリのサイズを抑えることが可能です。<br>
		\sa criFsBinder_BindCpk, criFsBinder_BindFile, criFsBinder_BindFiles, criFsBinder_BindDirectory, criFsBinder_Unbind
	*/
	CriSint32 max_binds;

	/*JP
		\brief 最大同時オープンファイル数
		\par 説明:
		アプリケーション中でオープンするファイルの数を指定します。<br>
		アプリケーション中で ::criFsStdio_OpenFile 関数等を使用してファイルをオープンする場合、
		本パラメータにオープンするファイルの数を指定する必要があります。<br>
		<br>
		max_filesには「同時にオープンするファイルの最大数」を指定します。<br>
		例えば、 ::criFsStdio_OpenFile 関数と ::criFsStdio_CloseFile 関数を交互に続けて実行するケースにおいては、
		最大同時には1つのファイルしかオープンしないため、関数の呼び出し回数に関係なくmax_filesに1を指定することが可能です。<br>
		逆に、ある場面でファイルを10個オープンする場合には、その他の場面でファイルを1つしかオープンしない場合であっても、
		max_filesに10を指定する必要があります。<br>
		\par 補足:
		CRI File Systemライブラリは、以下の関数を実行した場合にファイルをオープンします。<br>
		## ファイルがオープンされる場面
		|関数					|備考	|
		| --- | --- |
		|criFsBinder_BindCpk	|オープンされるファイルの数は1つ。<br> criFsBinder_Unbind 関数が実行されるまでの間ファイルはオープンされ続ける。	|
		|criFsBinder_BindFile	|オープンされるファイルの数は1つ。<br> criFsBinder_Unbind 関数が実行されるまでの間ファイルはオープンされ続ける。	|
		|criFsBinder_BindFiles	|リストに含まれる数分ファイルがオープンされる。<br> criFsBinder_Unbind 関数が実行されるまでファイルはオープンされ続ける。	|
		|criFsLoader_Load		|オープンされるファイルの数は1つ。<br> ロードが完了するまでの間ファイルはオープンされ続ける。<br> バインダーを指定した場合、ファイルはオープンされない（バインダーが既にオープン済みのため）。	|
		|criFsStdio_OpenFile	|オープンされるファイルの数は1つ。<br> criFsStdio_CloseFile 関数が実行されるまでの間ファイルはオープンされ続ける。<br> バインダーを指定した場合、ファイルはオープンされない（バインダーが既にオープン済みのため）。	|
		|criFsInstaller_Copy	|オープンされるファイルの数は2つ。<br> ファイルコピーが完了するまでの間ファイルはオープンされ続ける。<br> バインダーを指定した場合、オープンされるファイルは1つになる（1つをバインダーが既にオープン済みのため）。	|
		\attention
		ブリッジライブラリを使用してADXライブラリや救声主ライブラリを併用する場合、
		ADXTハンドルやcriSsPlyハンドルは内部的にCriFsStdioハンドルを作成します。<br>
		そのため、ブリッジライブラリを使用する場合には、CRI File Systemライブラリ初期化時に
		max_filesにADXTハンドルやcriSsPlyハンドルの数を加えた値を指定してください。<br>
	*/
	CriSint32 max_files;

	/*JP
		\brief パスの最大長（バイト単位）
		\par 説明:
		アプリケーション中で指定するファイルパスの最大長を指定します。<br>
		アプリケーション中で ::criFsLoader_Load 関数等を使用してファイルにアクセスする場合、
		本パラメータにアプリケーションで使用するパス文字列の最大長を指定する必要があります。<br>
		<br>
		max_pathには「使用するパス文字列の最大数」を指定します。<br>
		ある場面で256バイトのファイルパスを使用する場合、その他の場面で32バイトのファイルパスしか使わない場合でも、
		max_pathには256を指定する必要があります。<br>
		\par 備考:
		パスの最大長には、終端のNULL文字を含んだ数を指定する必要があります。<br>
		（「文字数＋１バイト」の値を指定する必要があります。）<br>
		\attention
		PC等、ユーザーがアプリケーションを自由な場所にインストール可能な場合には、想定される最大サイズを max_path に指定する必要がありますので、ご注意ください。<br>
	*/
	CriSint32 max_path;

	/*JP
		\brief ライブラリバージョン番号
		\par 説明:
		CRI File Systemライブラリのバージョン番号です。<br>
		::criFs_SetDefaultConfig 関数により、本ヘッダーに定義されているバージョン番号が設定されます。<br>
		\attention
		アプリケーションでは、この値を変更しないでください。<br>
	*/
	CriUint32 version;

	/*JP
		\brief ライブラリバージョン文字列
		\par 説明:
		CRI File Systemライブラリのバージョン文字列です。<br>
		::criFs_SetDefaultConfig 関数により、本ヘッダーに定義されているバージョン文字列が設定されます。<br>
		\attention
		アプリケーションでは、この値を変更しないでください。<br>
	 */
	const CriChar8 *version_string;

	/*JP
		\brief CPKファイルのCRCチェックを行うかどうか
		\par 説明:
		CPKファイル内のCRC情報を使用し、データ整合性チェックを行うかをどうかを切り替えるフラグです。<br>
		本フラグを CRI_TRUE に設定すると、以下のタイミングでCRCチェックを行います。
		 - CPKバインド時にTOC情報のCRCチェック
		 - コンテンツファイルロード時にコンテンツファイル単位のCRCチェック
		CPKに付加されたCRC情報と、実際に読みこんだデータのCRCが一致しない場合、エラーとなります。
	 */
	CriBool enable_crc_check;
} CriFsConfig;

/*JP
 * \brief ファイルオープンエラー発生時のリトライ方法
 */
typedef enum CriFsOpenRetryModeTag {
	CRIFS_OPEN_RETRY_NONE = 0,			/**< リトライしない */
	CRIFS_OPEN_RETRY_INFINITE = -1,		/**< 無限にリトライする */
	/* enum be 4bytes */
	CRIFS_OPEN_RETRY_ENUM_BE_SINT32 = 0x7FFFFFFF
} CriFsOpenRetryMode;

/*JP
 * \brief ファイルリードエラー発生時のリトライ方法
 */
typedef enum CriFsReadRetryModeTag {
	CRIFS_READ_RETRY_NONE = 0,			/**< リトライしない */
	CRIFS_READ_RETRY_INFINITE = -1,		/**< 無限にリトライする */
	/* enum be 4bytes */
	CRIFS_READ_RETRY_ENUM_BE_SINT32 = 0x7FFFFFFF
} CriFsReadRetryMode;

/*JP
 * \brief デフォルトのパス区切り文字の設定
 */
typedef enum CriFsDefaultPathSeparatorTag {
	CRIFS_DEFAULT_PATH_SEPARATOR_PLATFORM_COMPATIBLE = 0,		/**< プラットフォーム標準のパス区切り文字に変換する */
	CRIFS_DEFAULT_PATH_SEPARATOR_NONE = 1,						/**< パス区切り文字を変換せずそのまま使用する */
	CRIFS_DEFAULT_PATH_SEPARATOR_ENUM_BE_SINT32 = 0x7FFFFFFF
} CriFsDefaultPathSeparator;

/*JP
 * \brief ファイルI/Oモードの設定 
 */
typedef enum CriFsFileIoModeTag {
	CRIFS_FILE_IO_MODE_DEFAULT = 0,				/**< 機種デフォルトのファイルI/Oモード */
	CRIFS_FILE_IO_MODE_SHARE_FILE_HANDLE = 1,	/**< ファイルハンドルを共有する */
	CRIFS_FILE_IO_MODE_OPEN_EVERY_TIME = 2,		/**< ファイルアクセスごとにファイルのオープンを行う */
	/* enum be 4bytes */
	CRIFS_FILE_IO_MODE_ENUM_BE_SINT32 = 0x7FFFFFFF
} CriFsFileIoMode;

/*JP
 * \brief メモリ確保関数
 * \ingroup FSLIB_CRIFS
 * \param[in]	obj		ユーザー指定オブジェクト
 * \param[in]	size	要求メモリサイズ（バイト単位）
 * \return		void*	確保したメモリのアドレス（失敗時はNULL）
 * \par 説明:
 * メモリ確保関数登録用のインターフェイスです。<br>
 * CRI File Systemライブラリがライブラリ内で行なうメモリ確保処理を、
 * ユーザー独自のメモリ確保処理に置き換えたい場合に使用します。<br>
 * \par 備考:
 * コールバック関数が実行される際には、sizeに必要とされるメモリのサイズがセット
 * されています。<br>
 * コールバック関数内でsize分のメモリを確保し、確保したメモリのアドレスを
 * 戻り値として返してください。<br>
 * 尚、引数の obj には、::criFs_SetUserMallocFunction 関数で登録したユーザー指定
 * オブジェクトが渡されます。<br>
 * メモリ確保時にメモリマネージャー等を参照する必要がある場合には、
 * 当該オブジェクトを ::criFs_SetUserMallocFunction 関数の引数にセットしておき、
 * 本コールバック関数の引数を経由して参照してください。<br>
 * \attention
 * メモリの確保に失敗した場合、エラーコールバックが返されたり、呼び出し元の関数が
 * 失敗する可能性がありますのでご注意ください。
 * \sa CriFsFreeFunc, criFs_SetUserMallocFunction
 */
typedef void *(CRIAPI *CriFsMallocFunc)(void *obj, CriUint32 size);

/*JP
 * \brief メモリ解放関数
 * \ingroup FSLIB_CRIFS
 * \param[in]	obj		ユーザー指定オブジェクト
 * \param[in]	mem		解放するメモリアドレス
 * \return				なし
 * \par 説明:
 * メモリ解放関数登録用のインターフェイスです。<br>
 * CRI File Systemライブラリ内がライブラリ内で行なうメモリ解放処理を、
 * ユーザー独自のメモリ解放処理に置き換えたい場合に使用します。<br>
 * \par 備考:
 * コールバック関数が実行される際には、memに解放すべきメモリのアドレスがセット
 * されています。<br>
 * コールバック関数内でmemの領域のメモリを解放してください。
 * 尚、引数の obj には、::criFs_SetUserFreeFunction 関数で登録したユーザー指定
 * オブジェクトが渡されます。<br>
 * メモリ確保時にメモリマネージャー等を参照する必要がある場合には、
 * 当該オブジェクトを ::criFs_SetUserFreeFunction 関数の引数にセットしておき、
 * 本コールバック関数の引数を経由して参照してください。<br>
 * \sa criFsMallocFunc, criFs_SetUserFreeFunction
 */
typedef void (CRIAPI *CriFsFreeFunc)(void *obj, void *mem);

/**
 * \brief CPKコンテンツファイルID
 * \ingroup FSLIB_CRIFS
 * \par 説明：
 * CPKコンテンツファイルIDは、CPKファイルに格納された個々のコンテンツファイルを識別するためのものです。<br>
 * ファイル名とIDの両方をもつCPKファイルは、32ビットのIDを使用することができます。
 * ファイル名を持たないCPKファイルは、16ビットのIDを使用することができます。
 * どちらの場合も、正の値を有効なIDとして使用できます。
 */
typedef CriSint32 CriFsFileId;

/*==========================================================================
 *      CriFsIo API
 *=========================================================================*/
/*JP
 * \brief デバイスID
 */
/*EN
 * \brief Device ID
 */
typedef enum CriFsDeviceIdTag {
	CRIFS_DEVICE_00 = 0,	/*JP< デフォルトデバイス */
	CRIFS_DEVICE_01,
	CRIFS_DEVICE_02,
	CRIFS_DEVICE_03,
	CRIFS_DEVICE_04,
	CRIFS_DEVICE_05,
	CRIFS_DEVICE_06,
	CRIFS_DEVICE_07,		/*JP< メモリ */
	CRIFS_DEVICE_MAX,

	CRIFS_DEVICE_INVALID	= -1,	/*JP< 無効 */

	/* enum be 4bytes */
	CRIFS_DEVICE_ENUM_BE_SINT32 = 0x7fffffff
} CriFsDeviceId;

/**
 * \brief デバイス情報
 */
/*EN
 * \brief Device Information
 */
typedef struct CriFsDeviceInfoTag {
	CriBool can_read;					/**< 読み込み可能なデバイスかどうか				*/
	CriBool can_write;					/**< 書き込み可能なデバイスかどうか				*/
	CriBool can_seek;					/**< シーク可能なデバイスかどうか					*/
	CriSint32 minimum_read_size;		/**< 読み込み可能な最小単位のサイズ				*/
	CriSint32 minimum_write_size;		/**< 書き込み可能な最小単位のサイズ				*/
	CriSint32 minimum_seek_size;		/**< シーク可能な最小単位のサイズ					*/
	CriSint32 read_buffer_alignment;	/**< 読み込み先バッファーに要求されるアライメント	*/
	CriSint32 write_buffer_alignment;	/**< 書き込み先バッファーに要求されるアライメント	*/
} CriFsDeviceInfo;

/*JP
 * \brief ファイルオープンモード
 */
/*EN
 * \brief File Opening Mode
 */
typedef enum {
	CRIFS_FILE_MODE_APPEND			= 0,	/*JP< 既存ファイルに追記								*/	/*EN< Appends to an existing file						*/
	CRIFS_FILE_MODE_CREATE			= 1,	/*JP< ファイルの新規作成（既存のファイルは上書き）		*/	/*EN< Creates a new file always							*/
	CRIFS_FILE_MODE_CREATE_NEW		= 2,	/*JP< ファイルの新規作成（上書き不可）					*/	/*EN< Creates a new file (Can not overwrite)			*/
	CRIFS_FILE_MODE_OPEN			= 3,	/*JP< 既存ファイルのオープン							*/	/*EN< Opens an existing file							*/
	CRIFS_FILE_MODE_OPEN_OR_CREATE	= 4,	/*JP< ファイルのオープン（存在しない場合は新規作成）	*/	/*EN< Opens a file if available (Or creates new file)	*/
	CRIFS_FILE_MODE_TRUNCATE		= 5,	/*JP< 既存ファイルを0Byteに切り詰めてオープン			*/	/*EN< Opens a file and truncates it						*/

	/* enum be 4bytes */
	CRIFS_FILE_MODE_ENUM_BE_SINT32 = 0x7FFFFFFF
} CriFsFileMode;

/*JP
 * \brief ファイルアクセス種別
 */
/*EN
 * \brief Kind of File Access
 */
typedef enum {
	CRIFS_FILE_ACCESS_READ			= 0,	/*JP< 読み込みのみ		*/	/*EN< Read Only			*/
	CRIFS_FILE_ACCESS_WRITE			= 1,	/*JP< 書き込みのみ		*/	/*EN< Write Only		*/
	CRIFS_FILE_ACCESS_READ_WRITE	= 2,	/*JP< 読み書き			*/	/*EN< Read and Write	*/

	/* enum be 4bytes */
	CRIFS_FILE_ACCESS_ENUM_BE_SINT32 = 0x7FFFFFFF
} CriFsFileAccess;

/*JP
 * \brief I/Oインターフェイスのエラーコード
 */
/*JP
 * \brief Error of I/O Interface
 */
typedef enum {
	CRIFS_IO_ERROR_OK				=   0,	/*JP< エラーなし */
	CRIFS_IO_ERROR_NG				=  -1,	/*JP< 一般エラー */
	CRIFS_IO_ERROR_TRY_AGAIN		=  -2,	/*JP< リトライすべき */

											/* 特殊 */
	CRIFS_IO_ERROR_NG_NO_ENTRY		= -11,	/*JP< 個別エラー（ファイル無し） */
	CRIFS_IO_ERROR_NG_INVALID_DATA	= -12,	/*JP< 個別エラー（データが不正） */

	/* enum be 4bytes */
	CRIFS_IO_ERROR_ENUM_BE_SINT32 = 0x7FFFFFFF
} CriFsIoError;

/*JP
 * \brief ファイルハンドル
 */
/*EN
 * \brief File Handle
 */
typedef void *CriFsFileHn;

/*JP
 * \brief I/Oインターフェイス
 */
/*EN
 * \brief I/O Interface
 */
typedef struct CriFsIoInterfaceTag {
	/*JP
	 * \brief ファイルの有無の確認
	 * \param[in]	path	ファイルのパス
	 * \param[out]	result	ファイルが存在するかどうか
	 * \return	CriFsIoError	エラーコード
	 * \par 説明:
	 * 指定されたファイルの有無を確認する関数です。<br>
	 * ファイルが存在する場合は CRI_TRUE を、
	 * 存在しない場合は CRI_FALSE を result にセットする必要があります。<br>
	 */
	CriFsIoError (CRIAPI *Exists)(const CriChar8 *path, CriBool *result);

	/*JP
	 * \brief ファイルの削除
	 * \param[in]	path	ファイルのパス
	 * \return	CriFsIoError	エラーコード
	 * \par 説明:
	 * 指定されたファイルを削除する関数です。<br>
	 * \par 備考:
	 * デバイスで書き込みを行なわない場合には、この関数を実装せず、
	 * 構造体のメンバーに CRI_NULL を指定することも可能です。<br>
	 */
	CriFsIoError (CRIAPI *Remove)(const CriChar8 *path);

	/*JP
	 * \brief ファイル名の変更
	 * \param[in]	old_path	リネーム前のファイルのパス
	 * \param[in]	new_path	リネーム後のファイルのパス
	 * \return	CriFsIoError	エラーコード
	 * \par 説明:
	 * ファイル名の変更を行なう関数です。<br>
	 * old_path で指定されたファイルを、 new_path にリネームします。<br>
	 * \par 備考:
	 * デバイスで書き込みを行なわない場合には、この関数を実装せず、
	 * 構造体のメンバーに CRI_NULL を指定することも可能です。<br>
	 */
	CriFsIoError (CRIAPI *Rename)(const CriChar8 *old_path, const CriChar8 *new_path);

	/*JP
	 * \brief ファイルのオープン
	 * \param[in]	path	ファイルのパス
	 * \param[in]	mode	ファイルオープンモード
	 * \param[in]	access	ファイルアクセス種別
	 * \param[out]	filehn	ファイルハンドル
	 * \return	CriFsIoError	エラーコード
	 * \par 説明:
	 * 指定されたファイルをオープンする関数です。<br>
	 * オープンに成功した場合、CriFsFileHn 型のファイルハンドルを返す必要があります。<br>
	 * \par 補足:
	 * CriFsFileHn は void ポインターとして定義されています。<br>
	 * 独自のファイル情報構造体を定義し、そのアドレスを CriFsFileHn 型にキャストして返してください。<br>
	 * 尚、ファイルオープン時にメモリの確保が必要な場合には、本関数内で動的にメモリの確保を行なってください。<br>
	 * \attention
	 * 戻り値のエラーコード（ CriFsIoError ）には、関数内で継続不能なエラーが発生した
	 * 場合に限り CRIFS_IO_ERROR_NG をセットしてください。<br>
	 * （ファイルのオープンに失敗した場合でも、アプリケーションで処理を継続可能な場合には
	 * filehn に NULL をセットし、CRIFS_IO_ERROR_OK を返す必要があります。）<br>
	 * また、ディスク挿入待ち等の理由により、関数が実行されたタイミングでオープン処理
	 * を実行できない場合、エラーコードとして CRIFS_IO_ERROR_TRY_AGAIN を返すことで、
	 * 一定時間後（約10ms後）に再度オープン処理をやり直すことが可能です。<br>
	 * （関数の実行タイミングを先送りすることが可能です。）
	 */
	CriFsIoError (CRIAPI *Open)(
		const CriChar8 *path, CriFsFileMode mode, CriFsFileAccess access, CriFsFileHn *filehn);

	/*JP
	 * \brief ファイルのクローズ
	 * \param[in]	filehn	ファイルハンドル
	 * \return	CriFsIoError	エラーコード
	 * \par 説明:
	 * 指定されたファイルハンドルをクローズする関数です。<br>
	 * ファイルオープン時に動的にメモリの確保を行なった場合は、クローズ時にメモリを解放してください。<br>
	 */
	CriFsIoError (CRIAPI *Close)(CriFsFileHn filehn);

	/*JP
	 * \brief ファイルサイズの取得
	 * \param[in]	filehn	ファイルハンドル
	 * \param[out]	file_size	ファイルサイズ
	 * \return	CriFsIoError	エラーコード
	 * \par 説明:
	 * 指定されたファイルハンドルから、当該ファイルのサイズを取得する関数です。<br>
	 * \attention
	 * この関数はメインスレッド上から直接実行される可能性があります。<br>
	 * そのため、この関数の中で長時間処理をブロックすることは避ける必要があります。<br>
	 * ファイルハンドルからファイルサイズを取得するのに時間がかかる場合には、
	 * ファイルオープン時にあらかじめファイルサイズを取得（ファイルハンドル内に保持）
	 * しておき、本関数実行時にその値を返すよう関数を実装してください。<br>
	 */
	CriFsIoError (CRIAPI *GetFileSize)(CriFsFileHn filehn, CriSint64 *file_size);

	/*JP
	 * \brief 読み込みの開始
	 * \param[in]	filehn	ファイルハンドル
	 * \param[in]	offset	読み込み開始位置
	 * \param[in]	read_size	読み込みサイズ
	 * \param[in]	buffer	読み込み先バッファー
	 * \param[in]	buffer_size	バッファーサイズ
	 * \return	CriFsIoError	エラーコード
	 * \par 説明:
	 * データの読み込みを開始する関数です。<br>
	 * offset で指定された位置から、 read_size で指定されたサイズ分だけデータを
	 * buffer に読み込みます。<br>
	 * 関数のインターフェイスとしては非同期I/O処理による実装を想定していますが、
	 * スレッドを使用する場合（スレッドモデルに CRIFS_THREAD_MODEL_MULTI を指定する場合）
	 * には、この関数を同期I/O処理を使って実装しても問題ありません。<br>
	 * （関数内でファイルの読み込みを完了するまで待っても問題ありません。）<br>
	 * \attention
	 * 実際に読み込めたサイズは、 GetReadSize 関数で返す必要があります。<br>
	 * 同期I/O処理により本関数を実装する場合でも、読み込めたサイズは GetReadSize 関数
	 * が実行されるまで、ファイルハンドル内に保持する必要があります。<br>
	 */
	CriFsIoError (CRIAPI *Read)(CriFsFileHn filehn, CriSint64 offset, CriSint64 read_size, void *buffer, CriSint64 buffer_size);

	/*JP
	 * \brief 読み込み完了チェック
	 * \param[in]	filehn	ファイルハンドル
	 * \param[out]	result	ファイルの読み込みが完了したかどうか
	 * \return	CriFsIoError	エラーコード
	 * \par 説明:
	 * ファイルの読み込みが完了したかどうかを確認する関数です。<br>
	 * ファイルの読み込みが完了した場合は CRI_TRUE を、
	 * 読み込み途中の場合は CRI_FALSE を result にセットする必要があります。<br>
	 * \attention
	 * result には、リード処理の成否に関係なく、リード処理が完了した時点
	 * （デバイスへのアクセスが終了した時点）で CRI_TRUE をセットする必要があります。<br>
	 * リードエラーが発生した場合でも、 result に CRI_TRUE をセットし、
	 * 関数の戻り値は CRIFS_IO_ERROR_OK を返してください。<br>
	 * （リード処理が成功したかどうかについては、 GetReadSize 関数で判別しています。）<br>
	 * result に CRI_FALSE を返す限りは、CRI File System
	 * ライブラリは他の読み込み要求を一切処理しません。<br>
	 * （リードエラー発生時に result に CRI_FALSE をセットし続けた場合、
	 * ファイルのロードができなくなったり、ハンドルの
	 * Destroy 関数から処理が復帰しなくなる可能性があります。<br>
	 */
	CriFsIoError (CRIAPI *IsReadComplete)(CriFsFileHn filehn, CriBool *result);

	/*JP
	 * \brief ファイル読み込みのキャンセル発行
	 * \param[in]	filehn	ファイルハンドル
	 * \return	CriFsIoError	エラーコード
	 * \par 説明:
	 * デバイス側のファイル読み込みに対してキャンセルを発行し、即時に復帰する関数です。
	 * 戻り値は CRIFS_IO_ERROR_OK を返してください。<br>
	 * CRIFS_IO_ERROR_OK以外の値を返しても、
	 * CRI File Systemの動作はCRIFS_IO_ERROR_OKを返した場合と同じです。<br>
	 */
	CriFsIoError (CRIAPI *CancelRead)(CriFsFileHn filehn);

	/*JP
	 * \brief 読み込みサイズの取得
	 * \param[in]	filehn	ファイルハンドル
	 * \param[out]	read_size	読み込めたサイズ
	 * \return	CriFsIoError	エラーコード
	 * \par 説明:
	 * リード処理を行なった結果、実際にバッファーに読み込めたデータのサイズを返す関数です。<br>
	 * ファイルの終端等では、 Read 関数で指定したサイズ分のデータが必ずしも読み込めるとは限りません。<br>
	 * \attention
	 * リードエラーが発生した場合、 read_size に -1 をセットし、
	 * 関数の戻り値は CRIFS_IO_ERROR_OK を返してください。
	 */
	CriFsIoError (CRIAPI *GetReadSize)(CriFsFileHn filehn, CriSint64 *read_size);

	/*JP
	 * \brief 書き込みの開始
	 * \param[in]	filehn	ファイルハンドル
	 * \param[in]	offset	書き込み開始位置
	 * \param[in]	write_size	書き込みサイズ
	 * \param[in]	buffer	書き込み先バッファー
	 * \param[in]	buffer_size	バッファーサイズ
	 * \return	CriFsIoError	エラーコード
	 * \par 説明:
	 * データの書き込みを開始する関数です。<br>
	 * offset で指定された位置から、 write_size で指定されたサイズ分だけデータを
	 * buffer から書き込みます。<br>
	 * 関数のインターフェイスとしては非同期I/O処理による実装を想定していますが、
	 * スレッドを使用する場合（スレッドモデルに CRIFS_THREAD_MODEL_MULTI を指定する場合）
	 * には、この関数を同期I/O処理を使って実装しても問題ありません。<br>
	 * （関数内でファイルの書き込みを完了するまで待っても問題ありません。）<br>
	 * \attention
	 * 実際に書き込めたサイズは、 GetWriteSize 関数で返す必要があります。<br>
	 * 同期I/O処理により本関数を実装する場合でも、書き込めたサイズは GetWriteSize 関数
	 * が実行されるまで、ファイルハンドル内に保持する必要があります。<br>
	 * \par 備考:
	 * デバイスで書き込みを行なわない場合には、この関数を実装せず、
	 * 構造体のメンバーに CRI_NULL を指定することも可能です。<br>
	 */
	CriFsIoError (CRIAPI *Write)(CriFsFileHn filehn, CriSint64 offset, CriSint64 write_size, void *buffer, CriSint64 buffer_size);

	/*JP
	 * \brief 書き込み完了チェック
	 * \param[in]	filehn	ファイルハンドル
	 * \param[out]	result	ファイルの書き込みが完了したかどうか
	 * \return	CriFsIoError	エラーコード
	 * \par 説明:
	 * ファイルの書き込みが完了したかどうかを確認する関数です。<br>
	 * ファイルの書き込みが完了した場合は CRI_TRUE を、
	 * 書き込み途中の場合は CRI_FALSE を result にセットする必要があります。<br>
	 * \attention
	 * ライトエラーが発生した場合、 result に CRI_TRUE をセットし、
	 * 関数の戻り値は CRIFS_IO_ERROR_OK を返してください。
	 * \par 備考:
	 * デバイスで書き込みを行なわない場合には、この関数を実装せず、
	 * 構造体のメンバーに CRI_NULL を指定することも可能です。<br>
	 * \attention
	 * result には、ライト処理の成否に関係なく、ライト処理が完了した時点
	 * （デバイスへのアクセスが終了した時点）で CRI_TRUE をセットする必要があります。<br>
	 * ライトエラーが発生した場合でも、 result に CRI_TRUE をセットし、
	 * 関数の戻り値は CRIFS_IO_ERROR_OK を返してください。<br>
	 * （ライト処理が成功したかどうかについては、 GetReadSize 関数で判別しています。）<br>
	 * result に CRI_FALSE を返す限りは、CRI File System
	 * ライブラリは他の読み込み要求を一切処理しません。<br>
	 * （ライトエラー発生時に result に CRI_FALSE をセットし続けた場合、
	 * ファイルのロードができなくなったり、ハンドルの
	 * Destroy 関数から処理が復帰しなくなる可能性があります。<br>
	 */
	CriFsIoError (CRIAPI *IsWriteComplete)(CriFsFileHn filehn, CriBool *result);

	/*JP
	 * \brief ファイル書き込みのキャンセル発行
	 * \param[in]	filehn	ファイルハンドル
	 * \return	CriFsIoError	エラーコード
	 * \par 説明:
	 * デバイス側のファイル書き込みに対してキャンセルを発行し、即時に復帰する関数です。
	 * 戻り値は CRIFS_IO_ERROR_OK を返してください。<br>
	 * CRIFS_IO_ERROR_OK以外の値を返しても、
	 * CRI File Systemの動作はCRIFS_IO_ERROR_OKを返した場合と同じです。<br>
	 */
	CriFsIoError (CRIAPI *CancelWrite)(CriFsFileHn filehn);

	/*JP
	 * \brief 書き込みサイズの取得
	 * \param[in]	filehn	ファイルハンドル
	 * \param[out]	write_size	書き込めたサイズ
	 * \return	CriFsIoError	エラーコード
	 * \par 説明:
	 * ライト処理を行なった結果、実際にバッファーに読み込めたデータのサイズを返す関数です。<br>
	 * \attention
	 * ライトエラーが発生した場合、 write_size に -1 をセットし、
	 * 関数の戻り値は CRIFS_IO_ERROR_OK を返してください。
	 * \par 備考:
	 * デバイスで書き込みを行なわない場合には、この関数を実装せず、
	 * 構造体のメンバーに CRI_NULL を指定することも可能です。<br>
	 */
	CriFsIoError (CRIAPI *GetWriteSize)(CriFsFileHn filehn, CriSint64 *write_size);

	/*JP
	 * \brief フラッシュの実行
	 * \param[in]	filehn	ファイルハンドル
	 * \return	CriFsIoError	エラーコード
	 * \par 説明:
	 * 書き込み用にバッファーリングされているデータを、
	 * 強制的にデバイスに書き出す処理を行う関数です。<br>
	 * （ ANSI C 標準の API では fflush 関数に相当する処理です。）<br>
	 * \par 備考:
	 * デバイスで書き込みを行なわない場合には、この関数を実装せず、
	 * 構造体のメンバーに CRI_NULL を指定することも可能です。<br>
	 */
	CriFsIoError (CRIAPI *Flush)(CriFsFileHn filehn);

	/*JP
	 * \brief ファイルサイズの変更
	 * \param[in]	filehn	ファイルハンドル
	 * \param[out]	size	ファイルサイズ
	 * \return	CriFsIoError	エラーコード
	 * \par 説明:
	 * ファイルのサイズを指定したサイズに変更する関数です。<br>
	 * \par 補足:
	 * 本関数は、DMA転送サイズの制限等によりデバイスへの書き込みがバイト単位で
	 * 行なえない場合に、ファイルサイズを補正するために使用します。<br>
	 * そのため、書き込みがバイト単位で可能なデバイスについては、この関数を実装せず、
	 * 構造体のメンバーに CRI_NULL を指定することも可能です。<br>
	 * \par 備考:
	 * デバイスで書き込みを行なわない場合には、この関数を実装せず、
	 * 構造体のメンバーに CRI_NULL を指定することも可能です。<br>
	 */
	CriFsIoError (CRIAPI *Resize)(CriFsFileHn filehn, CriSint64 size);

	/*JP
	 * \brief ネイティブファイルハンドルの取得
	 * \param[in]	filehn	ファイルハンドル
	 * \param[out]	native_filehn	ネイティブのファイルハンドル
	 * \return	CriFsIoError	エラーコード
	 * \par 説明:
	 * プラットフォームSDKで利用されるファイルのハンドルを取得する関数です。<br>
	 * 例えば、 ANSI C 標準の fopen 関数を使用してファイルをオープンした場合、
	 * native_filehn としてファイルポインター（ FILE * ）を返す必要があります。<br>
	 */
	CriFsIoError (CRIAPI *GetNativeFileHandle)(CriFsFileHn filehn, void **native_filehn);

	/*JP
	 * \brief 読み込みプログレス加算コールバックの設定
	 * \param[in]	filehn		ファイルハンドル
	 * \param[in]	callback    読み込みプログレス加算コールバック
	 * \param[in]	obj			内部オブジェクト
	 * \par 説明:
	 * 本関数は、::criFsLoader_GetProgress で得られる進捗を、単位読み込みサイズより
	 * 細かい粒度で更新させるための、読み込みプログレス加算コールバックを設定する関数です。<br>
	 * 本関数を実装しない場合や、本関数で渡されたコールバック関数を使用しない場合、
	 * ::criFsLoader_GetProgress で得られる進捗は、基本的に単位読み込みサイズ毎に更新されます。<br>
	 * 本関数を実装する場合は、渡されたコールバック関数を Read 関数内で呼び出してください。
	 * また、呼び出す際には第一引数に obj、第二引数にメモリへの読み込みが完了したサイズを
	 * バイト単位で渡してください。<br>
	 * 例えば、リード要求をデバイス内で 8192byte ずつに分割して読み込む場合は、
	 * 8192byte の読み込み完了毎に、第二引数に 8192 を渡して呼び出してください。<br>
	 * この、読み込みプログレス加算コールバック呼び出しによって ::criFsLoader_GetProgress
	 * で得られる進捗が更新されます。単位読み込みサイズより細かい粒度で更新を
	 * 行うことで ::criFsLoader_GetProgress で得られる進捗の粒度が細かくなります。<br>
	 * \par 備考:
	 * 読み込みリクエストより細かい粒度で読み込み進捗を取得できない場合は、
	 * 実装するメリットはありません。
	 * \sa ::criFsLoader_GetProgress
	 */
	CriFsIoError (CRIAPI *SetAddReadProgressCallback)(CriFsFileHn filehn, void(*callback)(void*, CriSint32), void* obj);

	/*JP
	 * \brief 複数の同時ファイルアクセス要求が可能かどうかの問い合わせ
	 * \param[out]	result	複数の同時ファイルアクセス要求が可能かどうか
	 * \par 説明:
	 * このI/Oインターフェースが複数の同時ファイルアクセス要求が可能であるかどうかを返す関数です。<br>
	 * 本関数を実装しない場合、不可能であるとみなされます。<br>
	 * 本関数を実装されていて、result が CRI_TRUE だった場合、
	 * criFsLoader は効率よく複数ファイルのロードを行うために並列でリード要求を行うようになります。<br>
	 * \par 備考:
	 * 並列でリード要求を行う場合、CriFsFileIoMode が CRIFS_FILE_IO_MODE_OPEN_EVERY_TIME である必要があります。
	 */
	CriFsIoError(CRIAPI *CanParallelRead)(CriBool *result);
} CriFsIoInterface, *CriFsIoInterfacePtr;

/*JP
 * \brief I/O選択コールバック関数
 * \param[in]	path	ファイルのパス
 * \param[out]	device_id	デバイスID
 * \param[out]	ioif	I/Oインターフェイス
 * \par 説明:
 * I/O選択コールバック関数は、CRI File SystemライブラリのI/O処理を、
 * ユーザーの独自I/Oインターフェースで置き換える際に使用します。<br>
 * 具体的には、ユーザーは ::CriFsSelectIoCbFunc 型の関数を実装し、
 * その関数を ::criFs_SetSelectIoCallback 関数にセットする必要があります。<br>
 * ::CriFsSelectIoCbFunc 関数は、入力されたファイルのパス（引数のpath）を解析し、
 * そのファイルが存在するデバイスのID（引数のdevice_id）と、
 * デバイスにアクセスするためのI/Oインターフェイス（引数のioif）を返す必要があります。<br>
 * \par 補足:
 * ライブラリがデフォルト状態で利用するI/Oインターフェイスは、 ::criFs_GetDefaultIoInterface 関数で取得可能です。<br>
 * 特定のファイルのみを独自のI/Oインターフェイスを処理したい場合には、
 * 他のファイルを全て ::criFs_GetDefaultIoInterface 関数で取得したI/Oインターフェイスで処理してください。<br>
 * \code
 * CriError
 * \endcode
 * \sa criFs_SetSelectIoCallback, criFs_GetDefaultIoInterface
 */
typedef CriError (CRIAPI *CriFsSelectIoCbFunc)(const CriChar8 *path, CriFsDeviceId *device_id, CriFsIoInterfacePtr *ioif);

/*==========================================================================
 *      CriFsBinder API
 *=========================================================================*/
struct CriFsBinderHnObjTag;
/*JP
 * \brief CriFsBinderハンドル
 * \ingroup FSLIB_BINDER
 * \par 説明：
 * バインダーとは、ファイルを効率良く扱うためのデータベースです。<br>
 * - CriFsBinderHn (バインダーハンドル)とバインド<br>
 * バインダーを利用するには、バインダーハンドル( CriFsBinderHn )を作成し、
 * CPKファイル／ファイル／ディレクトリをバインダーに結びつけます。
 * このバインダーへの結び付けをバインドと呼びます。<br>
 * バインダーを作成すると、バインダーハンドル( CriFsBinderHn )が取得されます。<br>
 * - CriFsBindId （バインドID）<br>
 * バインダーにバインドを行うと、バインドIDが作成されます。個々のバインドを識別するために使用します。<br>
 * - ファイルのバインドとアンバインド<br>
 * バインダーには、CPKファイルやファイル、ディレクトリをどのような組み合わせででもバインドできます。<br>
 * バインドした項目のバインド状態を解除することをアンバインドと呼びます。<br>
 * - 利用できるバインド数<br>
 * 作成できるバインダー数や同時にバインドできる最大数は、 CriFsConfig の
 * num_binders (バインダー数)や max_binds (同時バインド可能な最大数)で指定します。<br>
 * - CPKファイルのバインド<br>
 * CPKファイルに収納されている個々のファイル（コンテンツファイル）にアクセスするには、
 * CPKファイルをバインドする必要があります。<br>
 * CPKファイルのコンテンツファイルもバインドできます。元のCPKファイルをアンバインドした場合、
 * バインドされているコンテンツファイルもアンバインドされます（暗黙的アンバインド）。<br>
 * - バインダーのプライオリティ<br>
 * バインダーは、目的のファイルがどのバインドIDにあるのかを検索します。<br>
 * このバインドIDの検索順は、基本的にはバインドされた順番になりますが、バインドIDのプライオリティを
 * 操作することで、検索順を変更することができます。<br>
 * - バインダーとCriFsのAPI<br>
 * CriFsLoader, CriFsGroupLoader, CriFsBinderには、バインダーを引数に持つAPIがあります。
 * その際には、 CriFsBinderHn と CriFsBindId 、どちらを指定するのかに注意してください。
 */
typedef struct CriFsBinderHnObjTag *CriFsBinderHn;

/**
 * \brief CriFsBinder ID
 * \ingroup FSLIB_BINDER
 * \par 説明：
 * バインダーに対してバインドを行うと、 CriFsBindId (バインドID)が作成されます。<br>
 * バインドIDは、個々のバインドを識別するためのもので、値は符号なし32ビット値の範囲を
 * とります。<br>
 * この型の変数は、無効なバインドIDであることを意味する特別値 CRIFSBINDER_BID_NULL (ゼロ)
 * をとる場合もあります。<br>
 */
typedef CriUint32 CriFsBindId;

/**
 * \brief ファイル情報構造体
 * \ingroup FSLIB_BINDER
 * \par 説明：
 * criFsBinder_Find(ById) 関数の出力情報です。
 * 検索したファイルにアクセスするための情報を格納します。<br>
 * CPKファイルのコンテンツファイルである場合、pathはCPKファイル名、offsetはCPKファイル
 * 先頭からのオフセット位置となります。<br>
 * \sa criFsBinder_Find(), criFsBinder_FindById()
 */
typedef struct CriFsBinderFileInfoTag {
	CriFsFileHn filehn;		/**< ファイルハンドル */
	CriChar8 *path;			/**< パス名 */
	CriSint64 offset;		/**< ファイルの先頭からのオフセット位置 */
	CriSint64 read_size;   	/**< 読込サイズ（圧縮ファイルサイズ） */
	CriSint64 extract_size;	/**< 展開サイズ（元のファイルサイズ、非圧縮ならread_sizeと同値） */
	CriFsBindId binderid;	/**< バインドID（どのバインド先から見つけたかを示す） */
	CriUint32 reserved[1];	/**< 予約領域 */
} CriFsBinderFileInfo;

/**
 * \brief コンテンツファイル情報構造体
 * \ingroup FSLIB_BINDER
 * \par 説明：
 * criFsBinder_GetContentsFileInfoById 関数の出力情報です。<br>
 * 検索したCPKファイルのコンテンツファイルにアクセスするための情報を格納します。<br>
 * \sa criFsBinder_GetContentsFileInfoById()
 */
typedef struct CriFsBinderContentsFileInfoTag {
	CriChar8 *directory;	/**< ディレクトリ名 */
	CriChar8 *filename;		/**< ファイル名 */
	CriUint32 read_size; 	/**< 読込サイズ（ファイルサイズ） */
	CriUint32 extract_size;	/**< 展開サイズ（非圧縮の場合、read_sizeと同値） */
	CriUint64 offset;		/**< CPKファイルの先頭からのオフセット位置 */
	CriFsFileId id;			/**< ファイルID */
	CriChar8 *ustr;			/**< ユーザー文字列 */
} CriFsBinderContentsFileInfo;

/*JP
 * \brief バインダーステータス
 * \ingroup FSLIB_BINDER
 * \par 説明：
 * criFsBinder_GetStatus 関数で取得される、バインドIDの状態です。<br>
 * バインドが完了するまで、バインドした項目にアクセスすることはできません。<br>
 * バインド対象が存在しなかったり、バインドに必要なリソースが不足する場合は、
 * バインド失敗となります。<br>
 * バインド失敗時の詳しい情報はエラーコールバック関数で取得してください。
 * \sa criFsBinder_GetStatus()
 */
typedef enum {
	CRIFSBINDER_STATUS_NONE = 0,
	CRIFSBINDER_STATUS_ANALYZE,		/**< バインド処理中 */
	CRIFSBINDER_STATUS_COMPLETE,	/**< バインド完了 */
	CRIFSBINDER_STATUS_UNBIND,		/**< アンバインド処理中 */
	CRIFSBINDER_STATUS_REMOVED,		/**< アンバインド完了 */
	CRIFSBINDER_STATUS_INVALID,		/**< バインド無効 */
	CRIFSBINDER_STATUS_ERROR,		/**< バインド失敗 */

	/* enum be 4bytes */
	CRIFSBINDER_STATUS_ENUM_BE_SINT32 = 0x7FFFFFFF
} CriFsBinderStatus;

/**
 * \brief バインド種別
 * \ingroup FSLIB_BINDER
 * \par 説明：
 * 何がバインドされているのかを示します。<br>
*/
typedef enum {
	CRIFSBINDER_KIND_NONE = 0,		/**< なし							*/
	CRIFSBINDER_KIND_DIRECTORY,		/**< ディレクトリバインド			*/
	CRIFSBINDER_KIND_CPK,			/**< CPKバインド					*/
	CRIFSBINDER_KIND_FILE,			/**< ファイルバインド				*/
	CRIFSBINDER_KIND_FILES,			/**< 複数ファイルバインド			*/
	CRIFSBINDER_KIND_FILE_SECTION,	/**< ファイルセクションバインド	*/
	CRIFSBINDER_KIND_SYSTEM,		/**< バインダーシステム関連		*/

	/* enum be 4bytes */
	CRIFSBINDER_KIND_ENUM_BE_SINT32 = 0x7FFFFFFF
} CriFsBinderKind;

/**
 * \brief Cpkバインドエラー無効時のエラー種別
 * \ingroup FSLIB_BINDER
 * \par 説明：
 * Cpkバインドが無効になった際の原因を示します。<br>
*/
typedef enum {
	CRIFS_BINDCPK_ERROR_NONE = 0,		/**< エラー無し */
	CRIFS_BINDCPK_ERROR_DATA,			/**< データ不整合 */
	CRIFS_BINDCPK_ERROR_CANNOT_READ,	/**< 読めない（メディアが無いなど） */
	CRIFS_BINDCPK_ERROR_NONEXISTENT,	/**< (CPK)ファイルが無い（メディアは存在する）*/
	/* enum be 4bytes */
	CRIFS_BINDCPK_ENUM_BE_SINT32 = 0x7FFFFFFF
} CriFsBindCpkError;

/**
 * \brief バインダー情報
 * \ingroup FSLIB_BINDER
 * \par 説明：
 * バインダー情報取得APIの出力情報です。<br>
 * \sa criFsBinder_GetBinderIdInfo()
*/
typedef struct CriFsBinderInfoTag {
	CriFsBinderKind kind;				/**< バインダー種別 */
	CriFsBinderStatus status;			/**< バインダーステータス */
	CriSint32 priority;					/**< プライオリティ設定 */
	CriSint32 nfiles;					/**< ファイル数<br>
										  バインダー種別<br>
										  ::CRIFSBINDER_KIND_FILES:バインドファイル数<br>
										  ::CRIFSBINDER_KIND_CPK:コンテンツファイル数<br>
										  他：０
										*/
	const CriChar8 *path;				/**< Bind関数呼び出し時に渡されたパス名 */
	const CriChar8 *real_path;			/**< 実際にバインドする対象となったパス名 */
	const CriChar8 *current_directory;	/**< カレントディレクトリ設定 */
	CriFsBindId bndrid;				/**< 他バインダーを参照している場合の参照先バインドID<br>
											CPKのコンテンツファイルをファイルバインドした場合、
											CPKのバインドIDが入ります。 */
} CriFsBinderInfo;

/* ROMアドレスの無効値 */
#define CRIFS_ROM_ADDRESS_INVALID  0xFFFFFFFFFFFFFFFF

/*==========================================================================
 *      CriFsLoader API
 *=========================================================================*/
struct CriFsLoaderObjTag;
/**
 * \brief CriFsLoaderハンドル
 */
typedef struct CriFsLoaderObjTag *CriFsLoaderHn;

/**
 * \brief ロード終了コールバック関数
 */
typedef void (CRIAPI *CriFsLoaderLoadEndCbFunc)(void *obj, CriFsLoaderHn loader);

/*JP
 * \brief ロードステータス
 */
/*EN
 * \brief Loading Status
 */
typedef enum {
	CRIFSLOADER_STATUS_STOP,		/**< 停止中		*/	/*EN< Stopping			*/
	CRIFSLOADER_STATUS_LOADING,		/**< ロード中		*/	/*EN< Loading			*/
	CRIFSLOADER_STATUS_COMPLETE,	/**< 完了			*/	/*EN< Complete			*/
	CRIFSLOADER_STATUS_ERROR,		/**< エラー		*/	/*EN< Error				*/
	/* enum be 4bytes */
	CRIFSLOADER_STATUS_ENUM_BE_SINT32 = 0x7FFFFFFF
} CriFsLoaderStatus;

/*JP
 * \brief ローダープライオリティ
 */
/*EN
 * \brief Priority
 */
typedef enum {
	CRIFSLOADER_PRIORITY_HIGHEST 		= 2,	/*JP< 最高		*/	/*EN< Highest		*/
	CRIFSLOADER_PRIORITY_ABOVE_NORMAL 	= 1,	/*JP< 高		*/	/*EN< Above normal	*/
	CRIFSLOADER_PRIORITY_NORMAL 		= 0,	/*JP< 普通		*/	/*EN< Normal		*/
	CRIFSLOADER_PRIORITY_BELOW_NORMAL 	= -1,	/*JP< 低		*/	/*EN< Below normal	*/
	CRIFSLOADER_PRIORITY_LOWEST 		= -2,	/*JP< 最低		*/	/*EN< Lowest		*/
	/* enum be 4bytes */
	CRIFSLOADER_PRIORITY_ENUM_BE_SINT32 = 0x7FFFFFFF
} CriFsLoaderPriority;

/*==========================================================================
 *      Group Loader API
 *=========================================================================*/

/*JP
 * \brief 準備ファイル数の制限を設定するAPIで「無制限」を示す特別値
 * \ingroup FSLIB_GROUPLOADER
 * \par 説明：
 * criFsGroupLoader_LimitNumPreparingFiles 関数でこの値を指定した場合や、
 * criFsGroupLoader_LimitNumPreparingFiles 関数を使用しない場合、準備処理は
 * criFsGroupLoader_LoadBulk 関数内で完結させます。<br>
 * \sa criFsGroupLoader_LimitNumPreparingFiles()
 */
#define CRIFS_GROUPLOADER_NO_PREPARATION_LIMIT			0

/**
 * \brief CriFsGroupLoader ハンドル
 * \ingroup FSLIB_GROUPLOADER
 * \par 説明：
 * CPKファイルのグループ情報を利用するには、グループローダーを作成する必要があります。<br>
 * グループローダーを作成すると、グループローダーハンドルが返されます。<br>
 * グループローダーへのアクセスは、グループローダーハンドルを使用して行います。
 */
struct _CriFsGroupLoaderHnObj;
typedef struct _CriFsGroupLoaderHnObj *CriFsGroupLoaderHn;

/**
 * \brief グループファイル情報構造体
 * \ingroup FSLIB_GROUPLOADER
 * \par 説明：
 * グループローダーで扱われる個々のファイルに関する情報です。
*/
typedef	struct CriFsGroupFileInfoTag {
	CriChar8	*directory;     /**< コンテンツファイルのディレクトリ名    */
	CriChar8	*filename;		/**< コンテンツファイルのファイル名        */
	CriUint32	filesize;		/**< コンテンツファイルのファイルサイズ	*/
	void		*datapointer;	/**< コンテンツファイルのデータポインター	*/
	CriUint32	gfinfotag;		/**< コンテンツファイルの汎用タグ          */
	CriFsFileId	id;				/**< コンテンツファイルのID                */
	CriUint32	continue_num;	/**< コンテンツファイルの連続数            */
} CriFsGroupFileInfo;

/**
 * \brief グループロードコールバック関数
 * \ingroup FSLIB_GROUPLOADER
 * \param[in]	obj		ユーザー登録オブジェクト
 * \param[in]	gfinfo	読み込むファイルの情報
 * \return		gfinfoで示されるファイルを読み込むバッファー領域へのポインター（NULL:ロードのスキップ）
 * \par 説明：
 * グループロードコールバック関数は、グループローダー毎に設定します。<br>
 * グループロード時、ロードするファイル毎にロードの直前に呼出されます。<br>
 * 読み込むファイルのファイル名やサイズなどのファイル情報が gfinfoで渡されます。<br>
 * コールバック関数の返値はgfinfoで示されたファイルを読み込むバッファーへのポインターとなります。
 * この返値がNULLの場合、そのファイルは読み込まれません。<br>
 * グループロードコールバック関数が設定されている場合、CPKファイルに設定されているグループ情報の
 * 読込アドレスは使用されません。
 * \sa criFsGroupLoader_SetLoadStartCallback()
 */
typedef void *(CRIAPI *CriFsGroupLoaderLoadStartCbFunc)(void *obj, const CriFsGroupFileInfo *gfinfo);

/*==========================================================================
 *      Log Output API
 *=========================================================================*/
/*JP
 * \brief アクセスログ出力モード
 */
typedef enum {
	CRIFS_LOGOUTPUT_MODE_DEFAULT,
	/* enum be 4bytes */
	CRIFS_LOGOUTPUT_MODE_ENUM_BE_SINT32 = 0x7FFFFFFF
} CriFsLogOutputMode;

/**
 * \brief ログ出力関数
 */
typedef void (CRIAPI *CriFsLogOutputFunc)(void *obj, const char* format, ...);

/*==========================================================================
 *      CriFsStdio API
 *=========================================================================*/
/**
 * \brief CriFsStdioハンドル
 */
struct CriFsStdioObjTag;
typedef struct CriFsStdioObjTag *CriFsStdioHn;

/*JP
 * \brief ファイル上のシーク開始位置
 */
typedef enum {
	CRIFSSTDIO_SEEK_SET = 0, /**< ファイルの先頭 */
	CRIFSSTDIO_SEEK_CUR = 1, /**< 現在の読込位置 */
	CRIFSSTDIO_SEEK_END = 2, /**< ファイルの終端 */
	/* enum be 4bytes */
	CRIFSSTDIO_SEEK_ENUM_BE_SINT32 = 0x7FFFFFFF
} CRIFSSTDIO_SEEK_TYPE;

/*JP
 * \brief ファイル削除結果
 */
typedef enum {
	CRIFSSTDIO_NOT_EXECUTED = 0,		/**< 削除操作が行われなかった	*/
	CRIFSSTDIO_FILE_REMOVED = 1,		/**< 正常終了					*/
	CRIFSSTDIO_IO_ERROR_OCCURRD =2,		/**< I/Oエラーが発生			*/
	/* enum is 4bytes */
	CRIFSSTDIO_REMOVE_RESULT_ENUM_IS_4BYTE = 0x7FFFFFFF
} CriFsStdioRemoveResult;


/*==========================================================================
 *      Load Limiter API
 *=========================================================================*/
/*JP
 * \brief ロードリミッタ番号
 * \par 説明:
 * 　デフォルトの状態では、全てのローダーハンドルがリミッタ０番（リミッタ制限なし）に割り当てられています。<br>
 * 　リミッタを利用する場合、制限を加えるローダー、グループローダー、バッチローダーのハンドルにリミッタ１番を設定してください。<br>
 * 　そして、リミッタ１番のリミッタサイズを設定してください。<br>
 * \attention
 * ゲーム機向けではロードリミッタ機能は非サポートです。<br>
 */
typedef enum {
	CRIFS_LOADLIMITER_NO_0_DEFAULT		= 0,	/* ロードリミッタ番号０番 (リミッタ制限なし、デフォルト) */
	CRIFS_LOADLIMITER_NO_1				= 1,	/* ロードリミッタ番号１番 */
	/* enum be 4bytes */
	CRIFS_LOADLIMITER_NO_ENUM_BE_SINT32 = 0x7FFFFFFF
} CriFsLoadLimiterNo;

/*JP
 * \brief ロードリミッタサイズの「リミッタ制限なし」を示す特別値
 * \par 説明:
 * 　リミッタ０番のリミッタサイズは、固定的に「リミッタ制限なし」となっています。<br>
 * 　リミッタ１番のリミッタサイズは、初期値が「リミッタ制限なし」となっています。<br>
 * \attention
 * ゲーム機向けではロードリミッタ機能は非サポートです。<br>
 */
#define CRIFS_LOADLIMITER_SIZE_UNLIMITED	0x7FFFFFFF

/*JP
 * \brief ロードリミッタサイズのデフォルト値（リミッタ制限なし）
 * \attention
 * ゲーム機向けではロードリミッタ機能は非サポートです。<br>
 */
#define CRIFS_LOADLIMITER_SIZE_DEFAULT		CRIFS_LOADLIMITER_SIZE_UNLIMITED

/*JP
 * \brief リード要求回数の「制限なし」を示す特別値（デフォルト値）
 */
#define CRIFS_READ_REQUEST_NUM_UNLIMITED        0x7FFFFFFF

/***************************************************************************
 *      変数宣言
 *      Prototype Variables
 ***************************************************************************/

/***************************************************************************
 *      関数宣言
 *      Prototype Functions
 ***************************************************************************/
#ifdef __cplusplus
extern "C" {
#endif

/*==========================================================================
 *      CRI File System API
 *=========================================================================*/
/*JP
 * \brief デフォルトコンフィギュレーションのセット
 * \ingroup FSLIB_CRIFS
 * \param[in]	p_config	コンフィギュレーション
 * \par 説明:
 * ::criFs_InitializeLibrary 関数に設定するコンフィギュレーション（ ::CriFsConfig ）に、デフォルトの値をセットします。<br>
 * \par 補足:
 * コンフィギュレーションに設定する各パラメータを、アプリケーションで使用するハンドルの数に応じて調節することで、
 * ライブラリが必要とするメモリサイズを小さく抑えることが可能です。<br>
 * しかし、アプリケーション中で使用するハンドルの数が明確でない開発初期段階や、メモリサイズがタイトではないケースでは、
 * 本マクロを使用することによりで、初期化処理を簡略化することが可能です。<br>
 * \attention:
 * 本マクロでは、ほとんどのケースで必要充分な数のハンドルが確保できるよう、コンフィギュレーションの各パラメータに大きめの値をセットします。<br>
 * そのため、本マクロを使用した場合、ライブラリが必要とするワーク領域のサイズは大きくなりますので、ご注意ください。<br>
 * （メモリサイズがタイトなケースでは、本マクロでコンフィギュレーションを初期化した後、各パラメータを個別に調節することをオススメいたします。）<br>
 * \sa
 * CriFsConfig
*/
#define criFs_SetDefaultConfig(p_config)	\
{\
	(p_config)->thread_model		= CRIFS_CONFIG_DEFAULT_THREAD_MODEL;\
	(p_config)->num_binders			= CRIFS_CONFIG_DEFAULT_NUM_BINDERS;\
	(p_config)->num_loaders			= CRIFS_CONFIG_DEFAULT_NUM_LOADERS;\
	(p_config)->num_group_loaders	= CRIFS_CONFIG_DEFAULT_NUM_GROUP_LOADERS;\
	(p_config)->num_stdio_handles	= CRIFS_CONFIG_DEFAULT_NUM_STDIO_HANDLES;\
	(p_config)->num_installers		= CRIFS_CONFIG_DEFAULT_NUM_INSTALLERS;\
	(p_config)->max_binds			= CRIFS_CONFIG_DEFAULT_MAX_BINDS;\
	(p_config)->max_files			= CRIFS_CONFIG_DEFAULT_MAX_FILES;\
	(p_config)->max_path			= CRIFS_CONFIG_DEFAULT_MAX_PATH;\
	(p_config)->version				= CRI_FS_VERSION;\
	(p_config)->version_string		= CRI_FS_VER_NUM;\
	(p_config)->enable_crc_check	= CRI_FALSE;\
}

/**
 * \brief ワーク領域サイズの計算
 * \ingroup FSLIB_CRIFS
 * \param[in]	config	コンフィギュレーション
 * \param[out]	nbyte	ワーク領域サイズ
 * \return	CriError	エラーコード
 * \par 説明:
 * CRI File Systemライブラリを使用するために必要な、ワーク領域のサイズを取得します。<br>
 * \par 備考:
 * ワーク領域のサイズはコンフィギュレーション（ ::CriFsConfig ）の内容によって変化します。<br>
 * ライブラリに割り当てるメモリサイズを削減したい場合には、コンフィギュレーションのパラメータを適宜調節してください。<br>
 * config に NULL を指定した場合、デフォルトのコンフィギュレーションが適用されます。
 * \sa CriFsConfig
 */
CriError CRIAPI criFs_CalculateWorkSizeForLibrary(const CriFsConfig *config, CriSint32 *nbyte);

/**
 * \brief CRI File Systemの初期化
 * \ingroup FSLIB_CRIFS
 * \param[in]	config	コンフィギュレーション
 * \param[in]	buffer	ワーク領域
 * \param[in]	size	ワーク領域サイズ
 * \return	CriError	エラーコード
 * \par 説明:
 * CRI File Systemライブラリを初期化します。<br>
 * ライブラリの機能を利用するには、必ずこの関数を実行する必要があります。<br>
 * （ライブラリの機能は、本関数を実行後、 ::criFs_FinalizeLibrary 関数を実行するまでの間、利用可能です。）<br>
 * ライブラリを初期化する際には、ライブラリが内部で利用するためのメモリ領域（ワーク領域）
 * を確保する必要があります。<br>
 * ワーク領域を確保する方法には、以下の2通りの方法があります。<br>
 * <b>(a) User Allocator方式</b>：メモリの確保／解放に、ユーザーが用意した関数を使用する方法。<br>
 * <b>(b) Fixed Memory方式</b>：必要なメモリ領域を直接ライブラリに渡す方法。<br>
 * <br>
 * User Allocator方式を用いる場合、ユーザーはCRI File Systemライブラリにメモリ確保関数を登録しておきます。<br>
 * workにNULL、sizeに0を指定して本関数を呼び出すことで、
 * ライブラリは登録済みのメモリ確保関数を使用して必要なメモリを自動的に確保します。<br>
 * ユーザーがワーク領域を用意する必要はありません。<br>
 * 初期化時に確保されたメモリは、終了処理時（ ::criFs_FinalizeLibrary 関数実行時）に解放されます。<br>
 * <br>
 * Fixed Memory方式を用いる場合、ワーク領域として別途確保済みのメモリ領域を本関数に
 * 設定する必要があります。<br>
 * ワーク領域のサイズは ::criFs_CalculateWorkSizeForLibrary 関数で取得可能です。<br>
 * 初期化処理の前に ::criFs_CalculateWorkSizeForLibrary 関数で取得したサイズ分のメモリを予め
 * 確保しておき、本関数に設定してください。<br>
 * 尚、Fixed Memory方式を用いた場合、ワーク領域はライブラリの終了処理（ ::criFs_FinalizeLibrary 関数）
 * を行なうまでの間、ライブラリ内で利用され続けます。<br>
 * ライブラリの終了処理を行なう前に、ワーク領域のメモリを解放しないでください。<br>
 * \par 例:
 * 【User Allocator方式によるライブラリの初期化】<br>
 * User Allocator方式を用いる場合、ライブラリの初期化／終了の手順は以下の通りです。<br>
 * 	-# 初期化処理実行前に、 ::criFs_SetUserMallocFunction 関数と
 * ::criFs_SetUserFreeFunction 関数を用いてメモリ確保／解放関数を登録する。<br>
 * 	-# 初期化用コンフィグ構造体にパラメータをセットする。<br>
 * 	-# ::criFs_InitializeLibrary 関数で初期化処理を行う。<br>
 * （workにはNULL、sizeには0を指定する。）<br>
 * 	-# アプリケーション終了時に ::criFs_FinalizeLibrary 関数で終了処理を行なう。<br>
 * <br>具体的なコードは以下のとおりです。<br>
 * \code
 * // 独自のメモリ確保関数
 * void *user_malloc(void *obj, CriUint32 size)
 * {
 * 	void *mem;
 *
 * 	// メモリの確保
 * 	mem = malloc(size);
 *
 * 	return (mem);
 * }
 *
 * // 独自のメモリ解放関数を用意
 * void user_free(void *obj, void *mem)
 * {
 * 	// メモリの解放
 * 	free(mem);
 *
 * 	return;
 * }
 *
 * main()
 * {
 * 	CriFsConfig config;	// ライブラリ初期化用コンフィグ構造体
 * 		:
 * 	// 独自のメモリ確保関数を登録
 * 	criFs_SetUserMallocFunction(user_malloc, NULL);
 *
 * 	// 独自のメモリ解放関数を登録
 * 	criFs_SetUserFreeFunction(user_free, NULL);
 *
 * 	// ライブラリ初期化用コンフィグ構造体にデフォルト値をセット
 * 	criFs_SetDefaultConfig(&config);
 *
 * 	// ライブラリの初期化
 * 	// ワーク領域にはNULLと0を指定する。
 * 	// →必要なメモリは、登録したメモリ確保関数を使って確保される。
 * 	criFs_InitializeLibrary(&config, NULL, 0);
 * 		:
 * 	// アプリケーションのメイン処理
 * 		:
 * 	// アプリケーションを終了する際に終了処理を行う
 * 	// →初期化時に確保されたメモリは、登録したメモリ解放関数を使って解放される。
 * 	criFs_FinalizeLibrary();
 * 		:
 * }
 * \endcode
 * <br>
 * 【Fixed Memory方式によるライブラリの初期化】<br>
 * Fixed Memory方式を用いる場合、ライブラリの初期化／終了の手順は以下の以下の通りです。<br>
 * 	-# 初期化用コンフィグ構造体にパラメータをセットする。<br>
 * 	-# ライブラリの初期化に必要なワーク領域のサイズを、 ::criFs_CalculateWorkSizeForLibrary
 * 関数を使って計算する。<br>
 * 	-# ワーク領域サイズ分のメモリを確保する。<br>
 * 	-# ::criFs_InitializeLibrary 関数で初期化処理を行う。<br>
 * （workには確保したメモリのアドレスを、sizeにはワーク領域のサイズを指定する。）<br>
 * 	-# アプリケーション終了時に ::criFs_FinalizeLibrary 関数で終了処理を行なう。<br>
 * 	-# ワーク領域のメモリを解放する。<br>
 * <br>具体的なコードは以下のとおりです。<br>
 * \code
 * main()
 * {
 * 	CriFsConfig config;	// ライブラリ初期化用コンフィグ構造体
 * 	void *work;				// ワーク領域アドレス
 * 	CriSint32 size;		// ワーク領域サイズ
 * 		:
 * 	// ライブラリ初期化用コンフィグ構造体にデフォルト値をセット
 * 	criFs_SetDefaultConfig(&config);
 *
 * 	// ライブラリの初期化に必要なワーク領域のサイズを計算
 * 	criFs_CalculateWorkSizeForLibrary(&config, &size);
 *
 * 	// ワーク領域用にメモリを確保
 * 	work = malloc((size_t)size);
 *
 * 	// ライブラリの初期化
 * 	// →確保済みのワーク領域を指定する。
 * 	criFs_InitializeLibrary(&config, work, size);
 * 		:
 * 	// アプリケーションのメイン処理
 * 	// →この間、確保したメモリは保持し続ける。
 * 		:
 * 	// アプリケーションを終了する際に終了処理を行う
 * 	criFs_FinalizeLibrary();
 *
 * 	// 必要なくなったワーク領域を解放する
 * 	free(work);
 * 		:
 * }
 * \endcode

 * \par 備考:
 * ライブラリ使用中に確保できるハンドルの数（CriFsBinderやCriFsLoaderの数）は、
 * 初期化設定コンフィギュレーション（引数のconfig）で指定します。<br>
 * ライブラリが必要とするワーク領域のサイズも、コンフィギュレーション内容に応じて変化します。<br>
 * （ハンドル数を増やせば、必要なメモリのサイズも大きくなります。）<br>
 * config に NULL を指定した場合、デフォルトのコンフィギュレーションが適用されます。
 * \attention
 * 本関数を実行後、必ず対になる ::criFs_FinalizeLibrary 関数を実行してください。<br>
 * また、 ::criFs_FinalizeLibrary 関数を実行するまでは、本関数を再度実行することはできません。<br>
 * \sa CriFsConfig, criFs_CalculateWorkSizeForLibrary, criFs_FinalizeLibrary
 * \sa criFs_SetUserMallocFunction, criFs_SetUserFreeFunction
 */
/*EN
 * \brief Initialize the CRI File System
 * \ingroup FSLIB_CRIFS
 * \param[in]	config	Configuration
 * \param[in]	buffer	Work area
 * \param[in]	size	Size of work area
 * \return	CriError	Error information
 * \par
 * Initialize the CRI File System<br>
 * \sa criFs_FinalizeLibrary
 */
CriError CRIAPI criFs_InitializeLibrary(const CriFsConfig *config, void *buffer, CriSint32 size);

/**
 * \brief CRI File Systemの終了
 * \ingroup FSLIB_CRIFS
 * \return	CriError	エラーコード
 * \par 説明:
 * CRI File Systemライブラリを終了します。<br>
 * \attention
 * ::criFs_InitializeLibrary 関数実行前に本関数を実行することはできません。<br>
 * \sa criFs_InitializeLibrary
 */
/*EN
 * \brief Finalize the CRI File System
 * \ingroup FSLIB_CRIFS
 * \return	CriError	Error information
 * \par
 * Finalize the CRI File System<br>
 * \sa criFs_InitializeLibrary
 */
CriError CRIAPI criFs_FinalizeLibrary(void);

/*JP
 * \brief メモリ確保関数の登録
 * \ingroup FSLIB_CRIFS
 * \param[in]	func		メモリ確保関数
 * \param[in]	obj			ユーザー指定オブジェクト
 * \par 説明:
 * CRI File Systemライブラリにメモリ確保関数を登録します。<br>
 * CRI File Systemライブラリ内がライブラリ内で行なうメモリ確保処理を、
 * ユーザー独自のメモリ確保処理に置き換えたい場合に使用します。<br>
 * <br>
 * 本関数の使用手順は以下のとおりです。<br>
 * (1) ::CriFsMallocFunc インターフェイスに副ったメモリ確保関数を用意する。<br>
 * (2) ::criFs_SetUserMallocFunction 関数を使用し、CRI File Systemライブラリに対して
 * メモリ確保関数を登録する。<br>
 * <br>
 * 具体的なコードの例は以下のとおりです。
 * \par 例:
 * \code
 * // 独自のメモリ確保関数を用意
 * void *user_malloc(void *obj, CriUint32 size)
 * {
 * 	void *mem;
 *
 * 	// メモリの確保
 * 	mem = malloc(size);
 *
 * 	return (mem);
 * }
 *
 * main()
 * {
 * 		:
 * 	// メモリ確保関数の登録
 * 	criFs_SetUserMallocFunction(user_malloc, NULL);
 * 		:
 * }
 * \endcode
 * \par 備考:
 * 引数の obj に指定した値は、 ::CriFsMallocFunc に引数として渡されます。<br>
 * メモリ確保時にメモリマネージャー等を参照する必要がある場合には、
 * 当該オブジェクトを本関数の引数にセットしておき、コールバック関数で引数を経由
 * して参照してください。<br>
 * \attention
 * メモリ確保関数を登録する際には、合わせてメモリ解放関数（ ::CriFsFreeFunc ）を
 * 登録する必要があります。
 * \sa CriFsMallocFunc, criFs_SetUserFreeFunction
 */
void CRIAPI criFs_SetUserMallocFunction(CriFsMallocFunc func, void *obj);

/*JP
 * \brief メモリ解放関数の登録
 * \ingroup FSLIB_CRIFS
 * \param[in]	func		メモリ解放関数
 * \param[in]	obj			ユーザー指定オブジェクト
 * \par 説明:
 * CRI File Systemライブラリにメモリ解放関数を登録します。<br>
 * CRI File Systemライブラリ内がライブラリ内で行なうメモリ解放処理を、
 * ユーザー独自のメモリ解放処理に置き換えたい場合に使用します。<br>
 * <br>
 * 本関数の使用手順は以下のとおりです。<br>
 * (1) ::CriFsFreeFunc インターフェイスに副ったメモリ解放関数を用意する。<br>
 * (2) ::criFs_SetUserFreeFunction 関数を使用し、CRI File Systemライブラリに対して
 * メモリ解放関数を登録する。<br>
 * <br>
 * 具体的なコードの例は以下のとおりです。
 * \par 例:
 * \code
 * // 独自のメモリ解放関数を用意
 * void user_free(void *obj, void *mem)
 * {
 * 	// メモリの解放
 * 	free(mem);
 *
 * 	return;
 * }
 *
 * main()
 * {
 * 		:
 * 	// メモリ解放関数の登録
 * 	criFs_SetUserFreeFunction(user_free, NULL);
 * 		:
 * }
 * \endcode
 * \par 備考:
 * 引数の obj に指定した値は、 ::CriFsFreeFunc に引数として渡されます。<br>
 * メモリ確保時にメモリマネージャー等を参照する必要がある場合には、
 * 当該オブジェクトを本関数の引数にセットしておき、コールバック関数で引数を経由
 * して参照してください。<br>
 * \attention
 * メモリ解放関数を登録する際には、合わせてメモリ確保関数（ ::CriFsMallocFunc ）を
 * 登録する必要があります。
 * \sa CriFsFreeFunc, criFs_SetUserMallocFunction
 */
void CRIAPI criFs_SetUserFreeFunction(CriFsFreeFunc func, void *obj);

/**
 * \brief サーバー処理の実行
 * \ingroup FSLIB_CRIFS
 * \return	CriError	エラーコード
 * \par 説明:
 * CRI File Systemライブラリの内部状態を更新します。<br>
 * アプリケーションは、この関数を定期的（毎フレーム1回程度）に実行する必要があります。<br>
 * \attention
 * criFs_ExecuteMain を実行しない場合、ファイルのロードが進まない等の問題が発生する可能性があります。<br>
 */
CriError CRIAPI criFs_ExecuteMain(void);

/**
 * \brief ファイルアクセス処理の実行（非スレッド時環境向け）
 * \ingroup FSLIB_CRIFS
 * \return	CriError	エラーコード
 * \par 説明:
 * CRI File Systemライブラリのファイルアクセス処理を実行します。<br>
 * \attention
 * この関数は、スレッドを使用しない環境でCRI File Systemライブラリを使用する場合に呼び出す必要があります。<br>
 * スレッドを使用する環境では、この関数の代わりに、 criFs_ExecuteMain 関数を実行してください。<br>
 * \sa
 * criFs_ExecuteMain()
 */
CriError CRIAPI criFs_ExecuteFileAccess(void);

/**
 * \brief データ展開処理の実行（非スレッド時環境向け）
 * \ingroup FSLIB_CRIFS
 * \return	CriError	エラーコード
 * \par 説明:
 * CRI File Systemライブラリのデータ展開処理を実行します。<br>
 * \attention
 * この関数は、スレッドを使用しない環境でCRI File Systemライブラリを使用する場合に呼び出す必要があります。<br>
 * スレッドを使用する環境では、この関数の代わりに、 criFs_ExecuteMain 関数を実行してください。<br>
 * \sa
 * criFs_ExecuteMain()
 */
CriError CRIAPI criFs_ExecuteDataDecompression(void);

/**
 * \brief ファイルオープンエラー発生時のリトライ方法の設定
 * \ingroup FSLIB_CRIFS
 * \param[in]	mode	リトライ方法
 * \return	CriError	エラーコード
 * \par 説明:
 * ファイルのオープンに失敗した場合に、CRI File Systemライブラリ内でオープンのリトライを行なうかどうかを指定します。<br>
 * リトライ方法に CRIFS_OPEN_RETRY_INFINITE を指定した場合、ファイルがオープンできるまでCRI File Systemライブラリはオープン処理をリトライし続けます。<br>
 * CRIFS_OPEN_RETRY_NONE を指定した場合、CRI File Systemライブラリはリトライ処理を行なわず、ハンドルのステータスをエラー状態に遷移します。
 */
CriError CRIAPI criFs_SetOpenRetryMode(CriFsOpenRetryMode mode);

/**
 * \brief ファイルリードエラー発生時のリトライ方法の設定
 * \ingroup FSLIB_CRIFS
 * \param[in]	mode	リトライ方法
 * \return	CriError	エラーコード
 * ファイルのリードに失敗した場合に、CRI File Systemライブラリ内でリードのリトライを行なうかどうかを指定します。<br>
 * リトライ方法に CRIFS_READ_RETRY_INFINITE を指定した場合、ファイルがリードできるまでCRI File Systemライブラリはリード処理をリトライし続けます。<br>
 * CRIFS_READ_RETRY_NONE を指定した場合、CRI File Systemライブラリはリトライ処理を行なわず、ハンドルのステータスをエラー状態に遷移します。
 */
CriError CRIAPI criFs_SetReadRetryMode(CriFsReadRetryMode mode);

/**
 * \brief グループ優先区間の開始
 * \ingroup FSLIB_CRIFS
 * \return	CriError	エラーコード
 * \param[in]	groupname	グループ名
 * \param[in]	attrname	アトリビュート名
 * \par 説明:
 * グループ優先区間を開始します。<br>
 * 本関数実行後、 ::criFs_EndGroup 関数を実行するまでの間、指定したグループ内のファイルが優先的にロードされるようになります。<br>
 * （バインダー内に同じ名前のファイルが複数存在する場合でも、指定したグループ内のファイルが優先して選択されます。）<br>
 * 本関数を使用することにより、通常のローダーを使用する場合でも、グループ化の恩恵を受けることが可能になります。<br>
 * \attention
 * 複数のグループ優先区間を重複させることはできません。<br>
 * 本関数を実行後、必ず対になる ::criFs_EndGroup 関数を実行してください。
 * 本関数と ::criFs_BeginLoadRegion 関数を併用することはできません。<br>
 * （CRI File System Ver.2.02.00より、 ::criFs_BeginLoadRegion 関数は本関数を呼び出すマクロに変更されました。）<br>
 * \sa criFs_EndGroup, criFs_BeginLoadRegion
 */
CriError CRIAPI criFs_BeginGroup(const CriChar8 *groupname, const CriChar8 *attrname);

/**
 * \brief グループ優先区間の終了
 * \ingroup FSLIB_CRIFS
 * \return	CriError	エラーコード
 * \par 説明:
 * グループ優先区間を終了します。<br>
 * \attention
 * 本関数と ::criFs_EndLoadRegion 関数を併用することはできません。<br>
 * （CRI File System Ver.2.02.00より、 ::criFs_EndLoadRegion 関数は本関数を呼び出すマクロに変更されました。）<br>
 * \sa criFs_BeginGroup
 */
CriError CRIAPI criFs_EndGroup(void);

/**
 * \brief バインダー使用数の取得
 * \ingroup FSLIB_CRIFS
 * \param[out]	cur_num		現在使用中のバインダーの数
 * \param[out]	max_num		過去に最大同時に利用したバインダーの数
 * \param[out]	limit		利用可能なバインダーの上限数
 * \return	CriError	エラーコード
 * \par 説明:
 * バインダーの使用数に関する情報を取得します。<br>
 */
CriError CRIAPI criFs_GetNumUsedBinders(CriSint32 *cur_num, CriSint32 *max_num, CriSint32 *limit);

/**
 * \brief ローダー使用数の取得
 * \ingroup FSLIB_CRIFS
 * \param[out]	cur_num		現在使用中のローダーの数
 * \param[out]	max_num		過去に最大同時に利用したローダーの数
 * \param[out]	limit		利用可能なローダーの上限数
 * \return	CriError	エラーコード
 * \par 説明:
 * ローダーの使用数に関する情報を取得します。<br>
 */
CriError CRIAPI criFs_GetNumUsedLoaders(CriSint32 *cur_num, CriSint32 *max_num, CriSint32 *limit);

/**
 * \brief グループローダー使用数の取得
 * \ingroup FSLIB_CRIFS
 * \param[out]	cur_num		現在使用中のグループローダーの数
 * \param[out]	max_num		過去に最大同時に利用したグループローダーの数
 * \param[out]	limit		利用可能なグループローダーの上限数
 * \return	CriError	エラーコード
 * \par 説明:
 * グループローダーの使用数に関する情報を取得します。<br>
 */
CriError CRIAPI criFs_GetNumUsedGroupLoaders(CriSint32 *cur_num, CriSint32 *max_num, CriSint32 *limit);

/**
 * \brief CriFsStdioハンドル使用数の取得
 * \ingroup FSLIB_CRIFS
 * \param[out]	cur_num		現在使用中のCriFsStdioハンドルの数
 * \param[out]	max_num		過去に最大同時に利用したCriFsStdioハンドルの数
 * \param[out]	limit		利用可能なCriFsStdioハンドルの上限数
 * \return	CriError	エラーコード
 * \par 説明:
 * CriFsStdioハンドルの使用数に関する情報を取得します。<br>
 */
CriError CRIAPI criFs_GetNumUsedStdioHandles(CriSint32 *cur_num, CriSint32 *max_num, CriSint32 *limit);

/**
 * \brief インストーラー使用数の取得
 * \ingroup FSLIB_CRIFS
 * \param[out]	cur_num		現在使用中のインストーラーの数
 * \param[out]	max_num		過去に最大同時に利用したインストーラーの数
 * \param[out]	limit		利用可能なインストーラーの上限数
 * \return	CriError	エラーコード
 * \par 説明:
 * インストーラーの使用数に関する情報を取得します。<br>
 */
CriError CRIAPI criFs_GetNumUsedInstallers(CriSint32 *cur_num, CriSint32 *max_num, CriSint32 *limit);

/**
 * \brief バインド数の取得
 * \ingroup FSLIB_CRIFS
 * \param[out]	cur_num		現在バインド中の数
 * \param[out]	max_num		過去に最大同時にバインドした数
 * \param[out]	limit		バインド可能回数の上限値
 * \return	CriError	エラーコード
 * \par 説明:
 * バインド数に関する情報を取得します。<br>
 */
CriError CRIAPI criFs_GetNumBinds(CriSint32 *cur_num, CriSint32 *max_num, CriSint32 *limit);

/**
 * \brief オープンされたファイル数の取得
 * \ingroup FSLIB_CRIFS
 * \param[out]	cur_num		現在オープン中のファイルの数
 * \param[out]	max_num		過去に最大同時にオープンしたファイルの数
 * \param[out]	limit		オープン可能なファイルの上限値
 * \return	CriError	エラーコード
 * \par 説明:
 * ファイルのオープン数に関する情報を取得します。<br>
 */
CriError CRIAPI criFs_GetNumOpenedFiles(CriSint32 *cur_num, CriSint32 *max_num, CriSint32 *limit);

/*JP
 * \brief I/O選択コールバックの登録
 * \ingroup FSLIB_CRIFS
 * \param[in]	func	I/O選択コールバック
 * \return	CriError	エラーコード
 * \par 説明:
 * I/O選択コールバック関数（ ::CriFsSelectIoCbFunc ）を登録します。<br>
 * CRI File Systemライブラリはファイルにアクセスする際、まず初めに、そのファイルが存在するデバイスのID（ ::CriFsDeviceId ）と、
 * デバイスにアクセスするためのI/Oインターフェイス（ ::CriFsIoInterface ）を選択します。<br>
 * デフォルト状態では、デバイスIDとI/Oインターフェイスの選択はライブラリ内で暗黙的に行なわれますが、
 * 本関数を使用することで、デバイスIDとI/Oインターフェイスをユーザーが自由に指定することが可能になります。<br>
 * これにより、ユーザーが独自に作成したI/Oインターフェイスを使用してファイルにアクセスすることが可能になります。<br>
 * \code
 * // 独自のI/Oインターフェイスを定義
 * // 備考）構造体のメンバー関数はユーザーが独自に実装。
 * static CriFsIoInterface g_userIoInterface = {
 * 	userExists,
 * 	userRemove,
 * 	userRename,
 * 	userOpen,
 * 	userClose,
 * 	userGetFileSize,
 * 	userRead,
 * 	userIsReadComplete,
 * 	userGetReadSize,
 * 	userWrite,
 * 	userIsWriteComplete,
 * 	userGetWriteSize,
 * 	userFlush,
 * 	userResize,
 * 	userGetNativeFileHandle
 * };
 *
 * // I/O選択コールバック関数
 * CriError user_select_io_callback(
 * 	const CriChar8 *path, CriFsDeviceId *device_id, CriFsIoInterfacePtr *ioif)
 * {
 * 	// パスを解析し、デバイスのIDを特定する
 * 	if (strncmp(path, …) == 0) {
 * 		(*device_id) = CRIFS_DEVICE_～;
 * 	} else {
 * 		(*device_id) = CRIFS_DEFAULT_DEVICE;
 * 	}
 *
 * 	// ファイルアクセスに使用するI/Oインターフェイスを指定する
 * 	(*ioif) = g_userIoInterface;
 *
 * 	return (CRIERR_OK);
 * }
 *
 * int main(…)
 * {
 * 		：
 * 	// I/O選択コールバックを登録
 * 	criFs_SetSelectIoCallback(user_select_io_callback);
 * 		：
 * }
 * \endcode
 * \attention
 * コールバック関数は1つしか登録できません。<br>
 * 登録操作を複数回行った場合、既に登録済みのコールバック関数が、
 * 後から登録したコールバック関数により上書きされてしまいます。<br>
 * <br>
 * funcにNULLを指定するとことで登録済み関数の登録解除が行えます。<br>
 * \sa CriFsSelectIoCbFunc, criFs_GetDefaultIoInterface
 */
CriError CRIAPI criFs_SetSelectIoCallback(CriFsSelectIoCbFunc func);

/**
 * \brief デフォルトI/Oインターフェイスの取得
 * \ingroup FSLIB_CRIFS
 * \param[out]	ioif	I/Oインターフェイス
 * \return	CriError	エラーコード
 * \par 説明:
 * CRI File Systemライブラリがデフォルトで利用するI/Oインターフェイスを取得します。<br>
 * I/O選択コールバック（::CriFsSelectIoCbFunc ）内でデフォルトの処理をさせたい場合には、
 * 本関数で取得したI/Oインターフェイスを、出力値として返してください。<br>
 * \code
 * // 独自のI/Oインターフェイスを定義
 * // 備考）構造体のメンバー関数はユーザーが独自に実装。
 * static CriFsIoInterface g_userIoInterface = {
 * 	userExists,
 * 	userRemove,
 * 	userRename,
 * 	userOpen,
 * 	userClose,
 * 	userGetFileSize,
 * 	userRead,
 * 	userIsReadComplete,
 * 	userGetReadSize,
 * 	userWrite,
 * 	userIsWriteComplete,
 * 	userGetWriteSize,
 * 	userFlush,
 * 	userResize,
 * 	userGetNativeFileHandle
 * };
 *
 * // I/O選択コールバック関数
 * CriError user_select_io_callback(
 * 	const CriChar8 *path, CriFsDeviceId *device_id, CriFsIoInterfacePtr *ioif)
 * {
 * 	// パスを解析し、デバイスのIDを特定する
 * 	if (strncmp(path, …) == 0) {
 * 		(*device_id) = CRIFS_DEVICE_～;
 * 	} else {
 * 		(*device_id) = CRIFS_DEFAULT_DEVICE;
 * 	}
 *
 * 	// sample.binのみを独自インターフェイスで処理する
 * 	if (strcmp(path, "sample.bin") == 0) {
 * 		(*ioif) = g_userIoInterface;
 * 	} else {
 * 		// 他のファイルはデフォルトI/Oインターフェイスで処理
 * 		criFs_GetDefaultIoInterface(ioif);
 * 	}
 *
 * 	return (CRIERR_OK);
 * }
 *
 * int main(…)
 * {
 * 		：
 * 	// I/O選択コールバックを登録
 * 	criFs_SetSelectIoCallback(user_select_io_callback);
 * 		：
 * }
 * \endcode
 * \sa CriFsSelectIoCbFunc, criFs_SetSelectIoCallback
 */
CriError CRIAPI criFs_GetDefaultIoInterface(CriFsIoInterfacePtr *ioif);

/**
 * \brief
 * \ingroup FSLIB_CRIFS
 * \param[in]	id				デバイスID
 * \param[in]	enabled			デバイスを作成するか否か
 * \return	CriError	エラーコード
 * \par 説明:
 * リードデバイスの有効・無効の切り替えを行います。アプリケーション側で使用しないデバイスがある場合は本関数で無効にしてください<br>
 * デフォルトでは、すべてのデバイスが Enabled の状態です。<br>
 * 本関数で不要なデバイスを作成しないようにすることで、ファイルシステムスレッドの動作を減らすことができます。<br>
 * CRIFS_DEVICE_DEFAULT と CRIFS_DEVICE_MEMORY 以外にも、プラットフォームによっては固有のデバイスが作成されます。<br>
 * 第一引数には CRIFS_DEVICE_00 ～ CRIFS_DEVICE_07 の範囲の値を指定してください。<br>
 * プラットフォーム固有のヘッダーファイルに記載されたデバイスIDと当該デバイスで扱うことのできるファイルパスを確認の上、本関数をご利用ください。<br>
 * 本関数でデバイスを無効にした結果、どのデバイスも作成されない場合、ライブラリ初期化時にエラーコールバックが発生します。<br>
 * \note
 * 本関数は::criFs_InitializeLibrary 関数よりも前に呼び出してください。<br>
 * \sa CriFsDeviceId
 */
CriError CRIAPI criFs_SetReadDeviceEnabled(CriFsDeviceId id, CriBool enabled);

/**
 * \brief デバイス情報の取得
 * \ingroup FSLIB_CRIFS
 * \param[in]	id		デバイスID
 * \param[out]	info	デバイス情報
 * \return	CriError	エラーコード
 * \par 説明:
 * 指定したデバイスの情報を取得します。<br>
 * 指定したデバイスがファイルの書き込みに対応しているかどうかや、
 * 読み書きに使用するバッファーのアライメント調整が必要かどうかを確認することが可能です。<br>
 * \sa CriFsDeviceId, CriFsDeviceInfo, criFs_SetDeviceInfo
 */
CriError CRIAPI criFs_GetDeviceInfo(CriFsDeviceId id, CriFsDeviceInfo *info);

/**
 * \brief デバイス情報の設定
 * \ingroup FSLIB_CRIFS
 * \param[in]	id		デバイスID
 * \param[in]	info	デバイス情報
 * \return	CriError	エラーコード
 * \par 説明:
 * 指定したデバイスの情報を変更します。<br>
 * I/Oレイヤー差し替え時、I/Oレイヤー側でデバイスの制限を緩和できる場合等に使用します。<br>
 * \sa CriFsDeviceId, CriFsDeviceInfo, criFs_GetDeviceInfo
 */
CriError CRIAPI criFs_SetDeviceInfo(CriFsDeviceId id, CriFsDeviceInfo info);

/**
 * \brief メモリファイルパスの作成
 * \ingroup FSLIB_CRIFS
 * \param[in]	buffer		データアドレス
 * \param[in]	buffer_size	データサイズ
 * \param[out]	path		パス文字列格納領域
 * \param[in]	length		パス文字列格納領域サイズ（単位はバイト）
 * \return	CriError	エラーコード
 * \par 説明:
 * メモリ上に配置されたデータをファイルとしてアクセスするためのパス文字列を作成します。<br>
 * 第1引数（ buffer ）と第2引数（ buffer_size ）にデータの格納アドレスとデータサイズを指定することで、
 * 当該データをファイルとして扱うためのパスが第3引数（ path ）に格納されます。<br>
 * 尚、パス文字列を格納する領域のサイズは、第4引数（ length ）で指定します。<br>
 * \par 備考:
 * 本関数は、メモリ上にロードされたCPKファイルをバインドする際に利用します。<br>
 * CPKファイルをメモリ上にロード後、ロードした領域のアドレスとサイズを本関数でパス文字列に変換するし、
 * ::criFsBinder_BindCpk 関数等に指定することで、オンメモリCPKデータのバインドが可能になります。<br>
 * \par 例:
 * \code
 * 		：
 * 	// CPKファイルをメモリ上にロード
 * 	criFsLoader_Load(loader_hn, NULL, "sample.cpk"
 * 		0, cpk_file_size, buffer, buffer_size);
 *
 * 	// ファイル読み込み完了待ち
 * 	for (;;) {
 * 		criFsLoader_GetStatus(loader_hn, &loader_status);
 * 		if (loader_status == CRIFSLOADER_STATUS_COMPLETE) {
 * 			break;
 * 		}
 * 			：
 * 	}
 *
 * 	// オンメモリCPKデータのアドレスとサイズをパス文字列に変換
 * 	criFs_AddressToPath(buffer, buffer_size, path, length);
 *
 * 	// オンメモリCPKのバインド
 * 	criFsBinder_BindCpk(binder_hn, NULL, path,
 * 		bind_work, bind_work_size, &bind_id);
 * 		：
 * \endcode
 * \attention
 * パス文字列の格納領域サイズが小さい場合、本関数は失敗し、エラーを返します。<br>
 * 現状、32bitメモリ空間のみの環境では、パス文字列の格納領域は 28 バイト必要です。<br>
 * 64bitメモリ空間の環境では、 パス文字列の格納領域は 44 バイト必要です。<br>
 * \sa CriFsDeviceId, CriFsDeviceInfo, criFs_GetDeviceInfo
 */
CriError CRIAPI criFs_AddressToPath(
	const void *buffer, CriSint64 buffer_size, CriChar8 *path, CriSint32 length);

/**
 * \brief メモリファイルシステム用同期コピーサイズ上限の設定
 * \ingroup FSLIB_CRIFS
 * \param[in]	limit	同期コピーサイズ上限
 * \return	CriError	エラーコード
 * \par 説明:
 * 一定サイズ以下のメモリコピー処理を、メインスレッド側で実行するよう設定します。<br>
 * <br>
 * メモリファイルシステムを使用する場合（ ::criFs_AddressToPath
 * 関数で作成したパスへアクセスする場合）、データの転送（≒メモリコピー処理）
 * は低プライオリティのメモリコピー専用スレッド上で行なわれます。<br>
 * （マルチスレッドモデル以外の場合、サーバー処理内でコピー処理が行われます。）<br>
 * <br>
 * この仕組みにより、巨大なデータをコピーする場合でもコピー処理にCPU時間が占有されることがなくなるため、
 * コピー処理によりメインループがブロックされてしまう、といった現象を防止することができます。<br>
 * 反面、別スレッドで行われるコピー処理をポーリングする形になるため、
 * 小さなデータをコピーする際でも、ポーリング間隔に依存した遅延が発生することになります。<br>
 * <br>
 * 本関数で同期コピーサイズ上限を設定すると、設定されたサイズ以下のデータをコピーする際、
 * 別スレッドでコピー処理を行わず、その場でコピー処理を実行するよう動作が変更されます。<br>
 * （メインスレッドにコピー処理分の負荷がかかりますが、ロード完了までの時間が短縮されます。）<br>
 * \par 備考:
 * デフォルト設定では全てのコピー処理が別スレッドで実行されます。<br>
 * <br>
 * 本関数で同期コピー処理が有効になると、各モジュールの動作は以下のように変化します。<br>
 * <br>
 * ・CriFsLoader<br>
 * ::criFsLoader_Load 関数実行直後にローダーのステータスが CRIFSLOADER_STATUS_COMPLETE に遷移します。<br>
 * （ ::criFsLoader_Load 関数内でコピー処理を行います。）<br>
 * <br>
 * ・CriFsStdio<br>
 * ::criFsStdio_ReadFile 関数内でスリープしなくなります。<br>
 * （ ::criFsStdio_ReadFile 関数内でコピー処理を行います。）<br>
 * \attention
 * 上限値を大きく設定した状態で巨大なファイルをロードすると、
 * ::criFsLoader_Load 関数等で長時間処理がブロックされる恐れがあります。<br>
 * <br>
 * 閾値の判定はロード要求サイズに対して行っています。<br>
 * そのため、実際にコピーできるサイズが指定されたリミット以下のサイズであっても、
 * ::criFsLoader_Load 関数や ::criFsStdio_ReadFile
 * 関数に指定したロード要求サイズがリミットを超えている場合、別スレッドで処理されます。<br>
 * \sa
 * criFs_AddressToPath
 */
CriError CRIAPI criFs_SetMemoryFileSystemSyncCopyLimit(CriSint64 limit);

/**
 * \brief ロードリミッタのサイズの設定
 * \ingroup FSLIB_CRIFS_EMB
 * \param[in]	limiter_no		ロードリミッタ番号
 * \param[in]	limiter_size	ロードリミッタサイズ（byte/1サーバー周期）
 * \return	CriError			エラーコード
 * \par 説明:
 * １サーバー周期当たりの読み込みサイズの制限値を設定します。<br>
 * 共通のリミッタ番号を割り当てた全てのローダー、グループローダー、バッチローダーの合計読み込みサイズが、ここで設定するサイズに制限されます。<br>
 * 制限に達したリミッタ番号のロード処理は一時保留され、次回のサーバー周期で自動的に再開します。<br>
 * リミッタに計上する数値は、ロードリミッタの単位サイズに切り上げて積算します。<br>
 * 圧縮データを読み込む場合、オリジナルデータサイズではなく、圧縮データサイズをリミッタに計上します。<br>
 * 圧縮ファイルを分割読み込みできない環境では、圧縮ファイルを読み込むときリミッタの制限を越える場合があります。このような環境では、巨大な圧縮ファイルに対しては実質的にリミッタの制御が効かないことにご注意ください。<br>
 * 読み込み途中でもリミッタサイズを変更することができます。リミッタサイズをゼロとして一時停止することもできます。 <br>
 * リミッタ０番のサイズは設定できません。０番は制限のない通常のロード処理に割り当てられます。 <br>
 * 現状はリミッタ１番だけが設定可能です。<br>
 * リミッタサイズのデフォルト値は CRIFS_LOADLIMITER_SIZE_DEFAULT （リミッタ制限なし）となっています。<br>
 * \attention
 * ゲーム機向けではロードリミッタ機能は非サポートです。この関数は呼び出さないでください。<br>
 * \sa CriFsLoadLimiterNo criFs_SetLoadLimiterSize criFs_SetLoadLimiterUnit criFsLoader_SetLoadLimiter criFsGroupLoader_SetLoadLimiter criFsBatchLoader_SetLoadLimiter
 */
CriError CRIAPI criFs_SetLoadLimiterSize(CriFsLoadLimiterNo limiter_no, CriSint32 limiter_size);

/**
 * \brief ロードリミッタの単位サイズの設定
 * \ingroup FSLIB_CRIFS_EMB
 * \param[in]	limiter_no		ロードリミッタ番号
 * \param[in]	limiter_unit	ロードリミッタ単位サイズ（byte）
 * \return	CriError			エラーコード
 * \par 説明:
 * DMA転送単位サイズやROMページサイズなど、分割読み込みを許容する最小のサイズを設定してください。<br>
 * リミッタ０番の単位サイズは設定できません。０番は制限のない通常のロード処理に割り当てられます。 <br>
 * 現状はリミッタ１番だけが設定可能です。<br>
 * \attention
 * ゲーム機向けではロードリミッタ機能は非サポートです。この関数は呼び出さないでください。<br>
 * \sa CriFsLoadLimiterNo criFs_SetLoadLimiterSize criFs_SetLoadLimiterUnit criFsLoader_SetLoadLimiter criFsGroupLoader_SetLoadLimiter criFsBatchLoader_SetLoadLimiter
 */
CriError CRIAPI criFs_SetLoadLimiterUnit(CriFsLoadLimiterNo limiter_no, CriSint32 limiter_unit);

/**
 * \brief リード要求回数の制限
 * \ingroup FSLIB_CRIFS_EMB
 * \param[in] limit_num_read_request  １サーバー周期当たりのリード要求回数の上限
 * \return  CriError エラーコード
 * \par 説明：
 * １サーバー周期当たりのリード要求回数の上限を設定します。<br>
 * デフォルト値は CRIFS_READ_REQUEST_NUM_UNLIMITED（制限なし）となっています。<br>
 * リード要求毎にかかるCPU負荷が高いプラットホームにおいて、この関数を利用する
 * ことでファイルシステムが消費するCPU時間を制限して、他の負荷が高い処理のため
 * にCPU時間を確保することができます。<br>
 * この関数を利用した場合、副作用として、多数の小さなファイルを読み込む場合の
 * スループットが低下します。<br>
 */
CriError CRIAPI criFs_LimitNumReadRequest(CriSint32 limit_num_read_request);

/**
 * \brief デフォルトパス区切り文字の設定
 * \ingroup FSLIB_CRIFS
 * \param[in]	default_path_separator	デフォルトパス区切り文字設定
 * \return	CriError			エラーコード
 * \par 説明:
 * CRI File Systemライブラリ内部で標準として扱うパス区切り文字を設定します。<br>
 * ::CRIFS_DEFAULT_PATH_SEPARATOR_PLATFORM_COMPATIBLE を設定すると、パス区切り文字を自動的に
 * そのプラットフォームで標準とされるパス区切り文字へと変換して処理します。<br>
 * ::CRIFS_DEFAULT_PATH_SEPARATOR_NONE を設定すると、パス区切り文字の変換は行わず、
 * 与えられたパスをそのまま使用するようになります。<br>
 * 未設定時（CRI File Systemライブラリのデフォルト設定）は::CRIFS_DEFAULT_PATH_SEPARATOR_PLATFORM_COMPATIBLE です。
 * \sa CriFsDefaultPathSeparator
 */
CriError CRIAPI criFs_SetDefaultPathSeparator(CriFsDefaultPathSeparator default_path_separator);

/**
 * \brief ファイルI/Oモードの設定
 * \ingroup FSLIB_CRIFS
 * \param[in]	io_mode ファイルI/Oモード	
 * \return	CriError			エラーコード
 * \par 説明:
 * CRI File Systemライブラリ全体のファイルI/Oモードを設定します。<br>
 * ::CRIFS_FILE_IO_MODE_SHARE_FILE_HANDLE を設定すると、ファイルハンドルをライブラリ内部で共有
 * し、ファイルアクセスを効率良く行います。<br>
 * 具体的には、::criFsBinder_BindCpk 関数、::criFsBinder_BindFile 関数を呼び出し時に作成
 * したファイルハンドルはアンバインドするまでライブラリ内部で保持し、保持中のファイルに対するアクセスでは
 * ファイルオープンが発生しません。<br>
 * <br>
 * ::CRIFS_FILE_IO_MODE_OPEN_EVERY_TIME を設定すると、ファイルハンドルの共有を行わずにファイル
 * アクセスのたびにファイルオープンを行います。<br>
 * ファイルオープン負荷の分だけファイル読み込みの性能は落ちますが、ファイルアクセスが必要な時のみ
 * ファイルハンドルを作成するため、ファイルディスクリプタなどのリソース消費を最小限に抑えることが可能です。<br>
 * <br>
 * 未設定時（CRI File Systemライブラリのデフォルト設定）は、機種ごとに異なります。<br>
 * 機種固有マニュアルに記載がない限り、デフォルト設定は ::CRIFS_FILE_IO_MODE_SHARE_FILE_HANDLE です。<br>
 * \par 注意：
 * 本関数はライブラリ初期化前に呼び出してください。<br>
 * ライブラリ初期化後に呼び出すことは出来ません。
 *
 * \sa CriFsFileIoMode
 */
CriError CRIAPI criFs_ControlFileIoMode(CriFsFileIoMode io_mode);

/**
 * \brief ファイルアクセススレッドのスタックサイズ設定
 * \ingroup FSLIB_CRIFS
 * \param[in]	size スタックサイズ（バイト単位）
 * \return	CriError			エラーコード
 * \par 説明:
 * ファイルアクセススレッドのスタックサイズを設定します。<br>
 * 設定しない場合は、機種ごとのデフォルト値が適用されます。<br>
 * \par 注意:
 * 本関数はライブラリ初期化前に呼び出してください。<br>
 * ライブラリ初期化後に呼び出すことは出来ません。<br>
 */
CriError CRIAPI criFs_SetFileAccessThreadStackSize(CriUint32 size);

/*==========================================================================
 *      CriFsIo API
 *=========================================================================*/

/*==========================================================================
 *      CriFsBinder API
 *=========================================================================*/
/*JP
 * \brief CRI File System - Binder オブジェクト
 * \ingroup FSLIB_BINDER
 * \par 説明:
 * CriFsBinderとはファイルデータをデータベース化するためのモジュールです。
 */

/**
 * \brief バインダーモジュール関数で使用されるメモリ管理関数の登録／削除
 * \ingroup FSLIB_BINDER
 * \param[in]	allocfunc	メモリ確保関数
 * \param[in]	freefunc	メモリ解放関数
 * \param[in]	obj			メモリ管理オブジェクト
 * \return		CriError	エラーコード
 * \par 説明：
 * CriFsBinder関数の引数で指定するワーク領域に NULL を指定した場合、内部から呼ばれるメモリ管理関数を登録します。<br>
 * CriFsBinder関数に NULL 以外を指定した場合は、渡された領域を利用します。<br>
 * <br>
 * 本関数により、CriFsBinder関数に必要なワーク領域の管理をユーザー独自のメモリ管理関数で動的に行うことが可能になります。<br>
 * 本関数によりメモリ管理関数を設定する場合は、 ::criFs_InitializeLibrary 関数の直後に本関数を呼び出してください。<br>
 * 本関数の引数allocfunc, freefuncがNULLの場合、登録されたメモリ管理関数を削除します。<br>
 * CriFsBinder関数は、現在登録されているメモリ管理関数を使用しますので、一旦登録されたメモリ管理関数を削除／変更する場合は、
 * 登録されたメモリ管理関数を利用して確保されたメモリ領域が無いことを確認するようにしてください。<br>
 * \par ::criFs_SetUserMallocFunction 関数との関係：
 * 本関数で登録されるメモリ管理関数はCriFsBinder関数での使用に限定されます。<br>
 * 本関数でメモリ管理関数を登録せず、::criFs_SetUserMallocFunction 関数で関数が登録されている場合は、
 * ::criFs_SetUserMallocFunction 関数で登録された関数がメモリ確保時に呼出されます。<br>
 * 本関数で登録された関数がある場合は、本関数で登録されたメモリ確保関数が呼出されます。<br>
 * \par CPKバインドについて：
 * CPKバインド時には、criFsBinder_GetWorkSizeForBindCpk関数の項にも記されてる通り、CPK解析に必要なワーク領域のサイズが
 * CPKの構成に依ってしまうので、事前に必要なワークサイズを見積ることが困難です。<br>
 * 本関数により、CPK解析エンジンが要求するメモリの管理をユーザー独自のメモリ管理関数で行うことが可能になります。<br>
 * \par 例：
 * \code
 * void *u_alloc(void *obj, CriUint32 size)
 * {
 *   :
 * }
 * void u_free(void *obj, void *ptr)
 * {
 *   :
 * }
 * void bind_cpk(void)
 * {
 *  CriFsBindId id;
 *  CriSint32 wksize;
 *  void *work;
 *  // メモリ管理関数の登録
 *  criFsBinder_SetUserHeapFunc(u_alloc, u_free, u_mem_obj);
 *
 *  // CPK バインド
 *  criFsBinder_BindCpk(NULL, NULL, "sample.cpk", NULL, 0, &id);
 *    :
 *
 *  // ワークを指定した場合、指定されたワークを使用します。
 *  criFsBinder_GetWorkSizeForBindFile(NULL, "sample.cpk", &wksize);
 *  work = malloc(wksize);
 *  criFsBinder_BindFile(NULL, NULL, "sample.cpk", work, wksize, &id);
 *    :
 *
 *  // CPKバインド時、criFsBinder_GetWorkSizeForBindCpk は CPK解析以外の
 *  // バインドに必要な最低限必要なワークのサイズを返しますので、
 *  // 未確定な領域だけを動的に確保するような使いかたも可能です。
 *  criFsBinder_GetWorkSizeForBindCpk(NULL, "sample.cpk", &wksize);
 *  work = malloc(wksize);
 *  // CPK バインド
 *  criFsBinder_BindCpk(NULL, NULL, "sample.cpk", work, wksize, &id);
 *    :
 * }
 * \endcode
 */
CriError CRIAPI criFsBinder_SetUserHeapFunc(CriFsMallocFunc allocfunc, CriFsFreeFunc freefunc, void *obj);

/**
 * \brief バインダーの生成
 * \ingroup FSLIB_BINDER
 * \param[out]	bndrhn		バインダーハンドル
 * \return		CriError	エラーコード
 * \par 説明：
 * バインダーを生成し、バインダーハンドルを返します。<br>
 * \par 例：
 * \code
 * CriFsBinderHn bndrhn;
 * criFsBinder_Create(&bndrhn);
 * 		:
 * criFsBinder_Destroy(bndrhn);
 * \endcode
 * \sa criFsBinder_Destroy()
 */
CriError CRIAPI criFsBinder_Create(CriFsBinderHn *bndrhn);

/**
 * \brief バインダーの破棄
 * \ingroup FSLIB_BINDER
 * \param[in]	bndrhn		バインダーハンドル
 * \return		CriError	エラーコード
 * \par 説明：
 * バインダーを破棄します。<br>
 * \par 注意：
 * 破棄するバインダーにバインドされているバインドIDも同時に破棄されます。<br>
 * 本関数で破棄できるのは、::criFsBinder_Create 関数により生成されたバインダーハンドルのみです。<br>
 * ::criFsBinder_GetHandle 関数により CriFsBindId から取得されたバインダーハンドルは破棄できません。<br>
 * CriFsBindId については ::criFsBinder_Unbind 関数をご使用ください。
 *
 * \sa criFsBinder_Create() criFsBinder_Unbind()
 */
CriError CRIAPI criFsBinder_Destroy(CriFsBinderHn bndrhn);

/**
 * \brief CPKファイルバインドのワークサイズ取得
 * \ingroup FSLIB_BINDER
 * \param[in]	srcbndrhn	バインドするCPKファイルにアクセスするためのバインダーハンドル
 * \param[in]	path		バインドするCPKファイルのパス名
 * \param[out]	worksize	必要ワークサイズ（バイト）
 * \return		CriError	エラーコード
 * \par 説明：
 * criFsBinder_BindCpk関数に指定するワークサイズを取得します。<br>
 * 本関数では、CPKの情報を解析するために最低限必要とされるワークサイズを取得できます。<br>
 * コンテンツファイル数の少いCPKファイルであれば、本関数で得られる数値のままでもCPKをバインドできる可能性があります。<br>
 * CPKバインド中にワークメモリが不足した場合、エラーコールバック関数で不足分の容量が示されます。<br>
 * <br>
 * *** CPKバインドに必要なワークサイズの取得するには、以下の方法があります。 ***<br><br>
 * １. エラーコールバックを利用する。<br>
 * CPKバインド中にワークメモリが不足した場合、エラーコールバック関数により不足分のサイズが示されます。<br>
 * 現在確保しているワーク領域のサイズに、エラーコールバックで得られるサイズを加算してワーク領域を再確保し、CPKバインドをやりなおします。<br>
 * CPKバインドをやりなおす場合、メモリ不足エラーコールバックを起こしたバインダーIDをアンバインドする必要があります。<br>
 * <br>
 * ２. メモリ確保／解放コールバック関数( ::criFsBinder_SetUserHeapFunc )を利用する。<br>
 * CPKバインド時にワークメモリの確保／解放が必要な際に呼出されるコールバック関数を登録します。<br>
 * バインド時に適時メモリ確保コールバック関数が呼出された場合、要求されたメモリを確保してエラーコールバック関数の返値として返します。<br>
 * <br>
 * ３. 必要ワークサイズ取得API（ ::criFsBinder_AnalyzeWorkSizeForBindCpk ）を利用する。<br>
 * バインドするCPKから情報を取得し、必要となるワークサイズを算出する関数を利用します。<br>
 * この関数は完了復帰関数で、関数内部でCPKファイルから情報を読み出しています。そのため関数終了までに時間がかかります。<br>
 * <br>
 * ４. CPKパッキングツールを利用する。<br>
 * パッキングツールにCPKファイルをドラッグ＆ドロップし、CPKファイルビューア［詳細］テキストボックスの
 * 「Enable Filename info.」「Enable ID info.」[Enable Group info.]項目に表示されるバイト数を、
 * 本関数で取得される必要ワークサイズに加算します。<br>
 * ただし、この方法で得られるワークサイズは目安ですので、メモリ不足エラーコールバックが起きる可能性があります。<br>
 * \sa criFsBinder_BindCpk() criFsBinder_SetUserHeapFunc() criFsBinder_AnalyzeWorkSizeForBindCpk()
 */
CriError CRIAPI criFsBinder_GetWorkSizeForBindCpk(CriFsBinderHn srcbndrhn, const CriChar8 *path, CriSint32 *worksize);

 /**
 * \brief CPKバインドに必要なワーク領域サイズの取得
 * \ingroup FSLIB_BINDER
 * \param[in]	srcbndrhn	バインダーハンドル
 * \param[in]	path		CPKファイルのパス
 * \param[in]	work		CPKヘッダー解析用のワーク領域
 * \param[in]	wksize	 	CPKヘッダー解析用のワーク領域のサイズ
 * \param[out]	rqsize		CPKバインドに必要なワーク領域のサイズ
 * \return		CriError	エラーコード
 * \par 説明：
 * srcbndrhn と path で指定されたCPKファイルを解析し、CPKバインドに必要なワーク領域の
 * サイズを取得します。<br>
 * 本関数は完了復帰関数です。<br>
 * 本関数は、指定されたCPKファイルのヘッダー情報を読み込んで解析しています。そのため、
 * 内部で読込待ちを行っています。
 * 本関数に渡すワーク領域は、criFsBinder_GetWorkSizeForBindCpk関数で得られたサイズの
 * ワーク領域を確保して渡してください。
 * \par 例：
 * \code
 * // ---- CPK解析に最低限必要なメモリの確保
 * criFsBinder_GetWorkSizeForBindCpk(bndrhn, path, &wksz)
 * work = malloc(wksz);
 * // CPKバインドに必要なメモリ容量を解析
 * criFsBinder_AnalyzeWorkSizeForBindCpk(bndrhn, path, work, wksz, &nbyte);
 * free(work);
 * // ----
 * // CPKバインド用メモリの確保
 * bindwork = malloc(nbyte);
 * // CPKバインド
 * criFsBinder_BindCpk(srcbndr, path, bindwork, nbyte, &bndrid);
 * \endcode
 */
CriError CRIAPI criFsBinder_AnalyzeWorkSizeForBindCpk(
	CriFsBinderHn srcbndrhn, const CriChar8 *path, void *work, CriSint32 wksize, CriSint32 *rqsize);

/*UNDESCRIBED
 * \brief CPKバインドに必要なワーク領域サイズの取得
 * \ingroup FSLIB_BINDER
 * \param[in]	tocsize     CPKファイルのTOC情報のサイズ(CPK_TOC_INFO_SIZE)
 * \param[in]	itocsize    CPKファイルのITOC情報のサイズ(CPK_ITOC_INFO_SIZE)
 * \param[in]	gtocsize    CPKファイルのGTOC情報のサイズ(CPK_GTOC_INFO_SIZE)
 * \param[out]	rqsize		CPKバインドに必要なワーク領域のサイズ(バイト)
 * \return		CriError	エラーコード
 * \par 説明：
 * CPKファイルバインドに必要なワーク領域のサイズを取得します。<br>
 * 本関数の引数は、CPKビルドの際に出力されるCPKヘッダーファイルのシンボル
 * CPK_TOC_INFO_SIZE、CPK_ITOC_INFO_SIZE、CPK_GTOC_INFO_SIZE で定義されている数値を
 * 指定します。<br>
 * 定義されていないシンボルは０を指定します。<br>
 * 例えば、IDのみCPKではCPK_TOC_INFO_SIZE、CPK_GTOC_INFO_SIZEが定義されていませんので、
 * tocsizeとgtosizeには０を指定します。<br>
 * \par 例：
 * \code
 *
 * // CPKバインドに必要なメモリ容量を解析
 * criFsBinder_CalculateWorkSizeForBindCpk(CPK_TOC_INFO_SIZE, CPK_ITOC_INFO_SIZE, CPK_GTOC_INFO_SIZE,&nbyte);
 * // ----
 * // CPKバインド用メモリの確保
 * bindwork = malloc(nbyte);
 * // CPKバインド
 * criFsBinder_BindCpk(srcbndr, path, bindwork, nbyte, &bndrid);
 * \endcode
 */
CriError CRIAPI criFsBinder_CalculateWorkSizeForBindCpk(CriSint32 tocsize, CriSint32 itocsize, CriSint32 gtocsize, CriSint32 *rqsize);

/**
 * \brief ファイルバインドのワークサイズの取得
 * \ingroup FSLIB_BINDER
 * \param[in]	srcbndrhn	バインドするファイルにアクセスするためのバインダーハンドル
 * \param[in]	path		バインドするファイルのパス名
 * \param[out]	worksize	必要ワークサイズ（バイト）
 * \return		CriError	エラーコード
 * \par 説明：
 * criFsBinder_BindFile関数に指定するワークサイズを取得します。
 * \sa criFsBinder_BindFile()
 */
CriError CRIAPI criFsBinder_GetWorkSizeForBindFile(CriFsBinderHn srcbndrhn, const CriChar8 *path, CriSint32 *worksize);

/**
 * \brief 複数ファイルバインドのワークサイズの取得
 * \ingroup FSLIB_BINDER
 * \param[in]	srcbndrhn	バインドするファイルにアクセスするためのバインダーハンドル
 * \param[in]	filelist	バインドするファイル名のリスト（セパレータ：',''\\t''\\n'  ターミネイタ:'\\0'）
 * \param[out]	worksize	必要ワークサイズ（バイト）
 * \return		CriError	エラーコード
 * \par 説明：
 * criFsBinder_BindFiles関数に指定するワークサイズを取得します。
 * \sa criFsBinder_BindFiles()
 */
CriError CRIAPI criFsBinder_GetWorkSizeForBindFiles(CriFsBinderHn srcbndrhn, const CriChar8 *filelist, CriSint32 *worksize);

/**
 * \brief ファイルセクションバインドのワークサイズの取得
 * \ingroup FSLIB_BINDER
 * \param[in]	srcbndrhn		バインドするファイルにアクセスするためのバインダーハンドル
 * \param[in]	path			バインドするファイルのパス名
 * \param[in]	section_name	セクション名
 * \param[out]	worksize		必要ワークサイズ（バイト）
 * \return		CriError		エラーコード
 * \par 説明：
 * criFsBinder_BindFileSection関数に指定するワークサイズを取得します。
 * \sa criFsBinder_BindFileSection()
 */
CriError CRIAPI criFsBinder_GetWorkSizeForBindFileSection(
	CriFsBinderHn srcbndrhn, const CriChar8 *path,
	const CriChar8 *section_name, CriSint32 *worksize);

/**
 * \brief ディレクトリバインドのワークサイズの取得
 * \ingroup FSLIB_BINDER
 * \param[in]	srcbndrhn	バインドするディレクトリにアクセスするためのバインダーハンドル
 * \param[in]	path		バインドするディレクトリのパス名
 * \param[out]	worksize	必要ワークサイズ（バイト）
 * \return		CriError	エラーコード
 * \par 説明：
 * criFsBinder_BindDirectory関数に指定するワークサイズを取得します。
 * \sa criFsBinder_BindDirectory()
 */
CriError CRIAPI criFsBinder_GetWorkSizeForBindDirectory(CriFsBinderHn srcbndrhn, const CriChar8 *path, CriSint32 *worksize);

/**
 * \brief Cpkファイルのバインド
 * \ingroup FSLIB_BINDER
 * \param[in]	bndrhn		バインド先のバインダーハンドル
 * \param[in]	srcbndrhn	バインドするCPKファイルにアクセスするためのバインダーハンドル
 * \param[in]	path		バインドする CPKファイルのパス名
 * \param[in]	work		バインド用（主にCPK解析）ワーク領域
 * \param[in]	worksize	ワーク領域のサイズ（バイト）
 * \param[out]	bndrid		バインドID
 * \return		CriError	エラーコード
 * \par 説明：
 * CPKファイルを利用するには、CPKファイルをバインドする必要があります。<br>
 * 本関数は、バインダー(bndrhn)にCPKファイル(path)をバインドし、バインドID(bndrid)を返します。<br>
 * srcbndrhnには、CPKファイルを検索するためのバインダーを指定します。
 * これがNULLの場合、デフォルトデバイスを使用します。<br>
 * ワーク領域(work)のサイズは、criFsBinder_GetWorkSizeForBindCpkで取得できます。
 * ワーク領域は、バインドIDが破棄されるまで保持してください。<br>
 * メモリ確保／解放コールバック関数が登録されている場合、ワーク領域にNULL(ワークサイズは０）を設定すると、
 * 必要なワーク領域をメモリ確保／解放コールバック関数を使用して動的に確保します。<br>
 * バインドを開始できない場合、バインドIDは CRIFSBINDER_BID_NULL が返されます。<br>
 * バインドIDに CRIFSBINDER_BID_NULL 以外が返された場合は内部リソースを確保していますので、
 * バインドの成功／失敗に関らず、不要になったバインドIDはアンバインドしてください。<br><br>
 * バインドしたCPKファイルはオープン状態で保持されます。
 * そのため、バインダー内部でCriFsLoaderを作成しています。<br><br>
 * 本関数は即時復帰関数です。本関数から復帰した直後は、CPKのバインドはまだ完了しておらず、
 * CPKのコンテンツファイルへのアクセスは行えません。<br>
 * バインド状態が完了（ CRIFSBINDER_STATUS_COMPLETE ）となった後に、CPKは利用可能となります。<br>
 * バインド状態は criFsBinder_GetStatus 関数で取得します。<br>
 * \par 例：
 * \code
 * void *work;
 * CriSint32 wksz;
 * CriFsBindId bndrid;
 * criFsBinder_GetWorkSizeForBindCpk(NULL, "smp.cpk",  &wksz);
 * work = malloc(wksz);
 * criFsBinder_BindCpk(bndrhn, NULL, "smp.cpk", work, wksz, &bndrid);
 * for (;;) {
 * 	CriFsBinderStatus status;
 * 	criFsBinder_GetStatus(bndrid, &status);
 * 	if (status == CRIFSBINDER_STATUS_COMPLETE) break;
 * }
 * \endcode
 * \sa criFsBinder_GetWorkSizeForBindCpk(), criFsBinder_SetUserHeapFunc(), criFsBinder_GetStatus(), criFsBinder_Unbind()
*/
CriError CRIAPI criFsBinder_BindCpk(CriFsBinderHn bndrhn, CriFsBinderHn srcbndrhn, const CriChar8 *path, void *work, CriSint32 worksize, CriFsBindId *bndrid);

/**
 * \brief ファイルのバインド
 * \ingroup FSLIB_BINDER
 * \param[in,out]	bndrhn	ファイルバインドをするバインダーハンドル
 * \param[in]	srcbndrhn	バインド対象のファイルを検索するためのバインダーハンドル
 * \param[in]	path		バインドするファイルのパス名
 * \param[in]	work		バインド用ワーク領域
 * \param[in]	worksize	ワーク領域のサイズ（バイト）
 * \param[out]	bndrid		バインドID
 * \return		CriError	エラーコード
 * \par 説明：
 * ファイルをバインドし、バインドIDを返します。<br>
 * srcbndrhnのバインダーからpathで指定されたファイルを検索し、bndrhnにバインドします。
 * srcbndrhnがNULLの場合、デフォルトデバイス上のファイルを検索します。<br>
 * ワーク領域(work)のサイズは、criFsBinder_GetWorkSizeForBindFileで取得できます。
 * ワーク領域は、バインドIDが破棄されるまで保持して下さい。<br>
 * メモリ確保／解放コールバック関数が登録されている場合、ワーク領域にNULL(ワークサイズは０）を設定すると、
 * 必要なワーク領域をメモリ確保／解放コールバック関数を使用して動的に確保します。<br>
 * バインドを開始できない場合、バインドIDは CRIFSBINDER_BID_NULL が返されます。
 * バインドIDに CRIFSBINDER_BID_NULL 以外が返された場合は内部リソースを確保していますので、
 * バインドの成功／失敗に関らず、不要になったバインドIDはアンバインドしてください。<br><br>
 * バインドされたファイルはファイルオープン状態で保持します。
 * このため、内部的にCriFsLoaderを作成しています。<br><br>
 * 本関数は即時復帰関数です。本関数から復帰した直後は、ファイルのバインドはまだ完了しておらず、
 * バインドIDを利用したファイルへのアクセスは行えません。<br>
 * バインドIDのバインド状態が完了（ CRIFSBINDER_STATUS_COMPLETE ）となった後に、
 * ファイルは利用可能となります。<br>
 * バインド状態は criFsBinder_GetStatus 関数で取得します。<br>
 * \par 例：
 * \code
 * void *work;
 * CriSint32 wksz;
 * CriFsBindId bndrid;
 * criFsBinder_GetWorkSizeForBindFile(NULL, "sample.txt", &wksz);
 * work = malloc(wksz);
 * criFsBinder_BindFile(bndrhn, NULL, "sample.txt", work, wksz, &bndrid);
 * for (;;) {
 * 	CriFsBinderStatus status;
 * 	criFsBinder_GetStatus(bndrid, &status);
 * 	if (status == CRIFSBINDER_STATUS_COMPLETE) break;
 * }
 * // データのロード (ファイルオープン状態でのアクセス)
 * criFsBinder_GetFileSize(binder, "sample.txt", &fsize); 
 * criFsLoader_Load(loader, binder, "sample.txt", 0, fsize, buffer, buffer_size); 
 * // ロード待ち... 
 * \endcode
 * \sa criFsBinder_GetWorkSizeForBindFile(), criFsBinder_SetUserHeapFunc(), criFsBinder_GetStatus(), criFsBinder_Unbind()
 */
CriError CRIAPI criFsBinder_BindFile(CriFsBinderHn bndrhn, CriFsBinderHn srcbndrhn, const CriChar8 *path, void *work, CriSint32 worksize, CriFsBindId *bndrid);

/**
 * \brief 複数ファイルのバインド
 * \ingroup FSLIB_BINDER
 * \param[in]	bndrhn	ファイルバインドをするバインダーハンドル
 * \param[in]	srcbndrhn	バインド対象のファイルを検索するためのバインダーハンドル
 * \param[in]	filelist	バインドするファイル名のリスト（セパレータ：',''\\t''\\n'  ターミネイタ:'\\0'）
 * \param[in]	work		バインド用ワーク領域
 * \param[in]	worksize	ワーク領域のサイズ
 * \param[out]	bndrid		バインドID
 * \return		CriError	エラーコード
 * \par 説明：
 * ファイルリスト(filelist)に列記されたファイルをバインドします。<br>
 * ファイルはsrcbndrhnから検索されますが、srcbndrhnがNULLの場合、デフォルトデバイスが使用されます。<br>
 * ワーク領域のサイズは、criFsBinder_GetWorkSizeForBindFiles で取得できます。
 * ワーク領域は、バインドIDが破棄されるまで保持して下さい。<br><br>
 * メモリ確保／解放コールバック関数が登録されている場合、ワーク領域にNULL(ワークサイズは０）を設定すると、
 * 必要なワーク領域をメモリ確保／解放コールバック関数を使用して動的に確保します。<br>
 * バインドを開始できない場合、バインドIDは CRIFSBINDER_BID_NULL が返されます。
 * バインドIDに CRIFSBINDER_BID_NULL 以外が返された場合は内部リソースを確保していますので、
 * バインドの成功／失敗に関らず、不要になったバインドIDはアンバインドしてください。<br><br>
 * バインドしたファイルはファイルオープン状態で保持します。
 * 内部的には CriFsLoader がバインドIDに１つ作成され、ファイルハンドルがバインドするファイル数分使用されます。<br><br>
 * 本関数は即時復帰関数です。本関数から復帰した直後は、ファイルのバインドはまだ完了しておらず、
 * バインダーを利用したファイルへのアクセスは行えません。<br>
 * バインド状態が完了（ CRIFSBINDER_STATUS_COMPLETE ）となった後に、ファイルは利用可能となります。<br>
 * バインド状態は criFsBinder_GetStatus 関数で取得します。<br>
 * \par 例：
 * \code
 * void *work;
 * CriSint32 wksz;
 * CriFsBindId bndrid;
 * CriChar8 *flist = "a.txt,b.txt,c.txt";
 * criFsBinder_GetWorkSizeForBindFiles(NULL, flist, &wksz);
 * work = malloc(wksz);
 * criFsBinder_BindFiles(bndrhn, NULL, flist, work, wksz, &bndrid);
 * for (;;) {
 * 	CriFsBinderStatus status;
 * 	criFsBinder_GetStatus(bndrid, &status);
 * 	if (status == CRIFSBINDER_STATUS_COMPLETE) break;
 * }
 * \endcode
 * \sa criFsBinder_GetWorkSizeForBindFiles(), criFsBinder_SetUserHeapFunc(), criFsBinder_GetStatus(), criFsBinder_Unbind()
 */
CriError CRIAPI criFsBinder_BindFiles(CriFsBinderHn bndrhn, CriFsBinderHn srcbndrhn, const CriChar8 *filelist, void *work, CriSint32 worksize, CriFsBindId *bndrid);

/**
 * \brief ファイルセクションのバインド
 * \ingroup FSLIB_BINDER
 * \param[in,out]	bndrhn		ファイルセクションバインドをするバインダーハンドル
 * \param[in]	srcbndrhn		バインド対象のファイルを検索するためのバインダーハンドル
 * \param[in]	path			バインドするファイルのパス名
 * \param[in]	offset			データの開始位置（バイト）
 * \param[in]	size			データサイズ（バイト）
 * \param[in]	section_name	セクション名
 * \param[in]	work			バインド用ワーク領域
 * \param[in]	worksize		ワーク領域のサイズ（バイト）
 * \param[out]	bndrid			バインドID
 * \return		CriError		エラーコード
 * \par 説明：
 * ファイルの一部分をバインドし、その箇所を仮想的なファイルとして扱えるよう設定します。<br>
 * srcbndrhnのバインダーからpathで指定されたファイルを検索してバインドします。
 * srcbndrhnがNULLの場合、デフォルトデバイスを使用します。<br>
 * ワーク領域(work)のサイズは、criFsBinder_GetWorkSizeForBindFileSectionで取得できます。
 * ワーク領域は、バインドIDが破棄されるまで保持して下さい。<br>
 * メモリ確保／解放コールバック関数が登録されている場合、ワーク領域にNULL(ワークサイズは０）を設定すると、
 * 必要なワーク領域をメモリ確保／解放コールバック関数を使用して動的に確保します。<br>
 * バインドを開始できない場合、バインドIDは CRIFSBINDER_BID_NULL が返されます。
 * バインドIDに CRIFSBINDER_BID_NULL 以外が返された場合は内部リソースを確保していますので、
 * バインドの成功／失敗に関らず、不要になったバインドIDはアンバインドしてください。<br><br>
 * バインドされたファイルはファイルオープン状態で保持します。
 * このため、内部的にCriFsLoaderを作成しています。<br><br>
 * 本関数は即時復帰関数です。本関数から復帰した直後は、ファイルのバインドはまだ完了しておらず、
 * バインドIDを利用したファイルへのアクセスは行えません。<br>
 * バインドIDのバインド状態が完了（ CRIFSBINDER_STATUS_COMPLETE ）となった後に、
 * ファイルは利用可能となります。<br>
 * バインド状態は criFsBinder_GetStatus 関数で取得します。<br>
 * \par 例：
 * \code
 * CriFsBindId binder_id;
 * CriFsBinderStatus status;
 * 	：
 * // sample.txtの100バイト目から5000バイト分をSTAGE1という名前でバインド
 * // 備考）アロケーターが事前に登録済みであればワーク領域サイズの指定は不要。
 * criFsBinder_BindFileSection(binder_hn, NULL, "sample.txt", 100, 5000, "STAGE1", NULL, 0, &binder_id);
 *
 * // バインド完了待ち
 * for (;;) {
 * 	// バインド状態のチェック
 * 	criFsBinder_GetStatus(binder_id, &status);
 * 	if (status == CRIFSBINDER_STATUS_COMPLETE) {
 * 		break;
 * 	}
 *
 * 	// サーバー処理の実行
 * 	criFs_ExecuteMain();
 *
 * 	// Vsync待ち等
 * 	…
 * }
 *
 * // バインド済みのセクションのデータを読み込み
 * // 備考）ロード時のパスにはセクション名を指定する。
 * criFsLoader_Load(loader_hn, binder_hn, "STAGE1", 0, 5000, buffer, buffer_size);
 * 	：
 * \endcode
 * \sa criFsBinder_GetWorkSizeForBindFileSection, criFsBinder_GetStatus, criFsBinder_Unbind
 */
CriError CRIAPI criFsBinder_BindFileSection(
	CriFsBinderHn bndrhn, CriFsBinderHn srcbndrhn, const CriChar8 *path,
	CriUint64 offset, CriSint32 size, const CriChar8 *section_name,
	void *work, CriSint32 worksize, CriFsBindId *bndrid);

/**
 * \brief ディレクトリパスのバインド
 * \ingroup FSLIB_BINDER
 * \param[in]	bndrhn		バインダーハンドル
 * \param[in]	srcbndrhn	バインドしたディレクトリ名でファイルにアクセスする際のバインダー
 * \param[in]	path		バインドするディレクトリパス名
 * \param[in]	work		バインド用ワーク領域
 * \param[in]	worksize	ワーク領域のサイズ（バイト）
 * \param[out]	bndrid		バインドID
 * \return		CriError	エラーコード
 * \par 説明：
 * ディレクトリパス名をバインドします。<br>
 * バインドするディレクトリ名は絶対パスで指定してください。
 * バインド時に指定されたディレクトリが存在するか否かはチェックしていません。<br>
 * バインドされるのはディレクトリパスだけで、指定されたディレクトリ内のファイルを
 * オープン状態にするものではありません。よってバインドに失敗しない限り、本関数から復帰時には
 * バインドIDのバインド状態は完了（ CRIFSBINDER_STATUS_COMPLETE ）となります。<br>
 * srcbndrhnには、本関数でバインドしたディレクトリを用いてファイルを検索する際に、
 * 検索対象となるバインダーを指定します。<br>
 * ワーク領域(work)のサイズは、criFsBinder_GetWorkSizeForBindDirectoryで取得できます。
 * ワーク領域は、バインドIDが破棄されるまで保持して下さい。<br>
 * メモリ確保／解放コールバック関数が登録されている場合、ワーク領域にNULL(ワークサイズは０）を設定すると、
 * 必要なワーク領域をメモリ確保／解放コールバック関数を使用して動的に確保します。<br>
 * バインドを開始できない場合、バインドIDは CRIFSBINDER_BID_NULL が返されます。
 * バインドIDに CRIFSBINDER_BID_NULL 以外が返された場合は内部リソースを確保していますので、
 * バインドの成功／失敗に関らず、不要になったバインドIDはアンバインドしてください。<br>
 * \par 備考：
 * 最大同時にバインド可能なディレクトリ数には制限があります。<br>
 * 具体的には、CriFsConfig の num_binders や max_binds の値が充分であっても、
 * 現状は最大16個（PCのみ64個）までしかディレクトリをバインドすることができません。<br>
 * \attention
 * 本関数は開発支援用のデバッグ関数です。<br>
 * 本関数を使用した場合、以下の問題が発生する可能性があります。<br>
 * - criFsLoader_Load関数やcriFsBinder_GetFileSize関数内で処理が長時間ブロックされる。<br>
 * - バインドしたディレクトリ内のファイルにアクセスする際、音声やムービーのストリーム再生が途切れる。<br>
 * 
 * <br>
 * マスターアップ時には本関数をアプリケーション中で使用しないようご注意ください。<br>
 * （ディレクトリ内のデータをCPKファイル化してcriFsBinder_BindCpk関数でバインドするか、またはディレクトリ内のファイルを全てcriFsBinder_BindFiles関数でバインドしてください。）<br>
 * \par 例：
 * \code
 * void *work;
 * CriSint32 wksz;
 * CriFsBindId bndrid;
 * criFsBinder_GetWorkSizeForBindDirectory(bndrhn, "/cri/samples/", &wksz);
 * work = malloc(wksz);
 * criFsBinder_BindDirectory(bndrhn, bndrhn, "/cri/samples/", work, wksz, &bndrid);
 * \endcode
 * \sa criFsBinder_GetWorkSizeForBindDirectory(), criFsBinder_SetUserHeapFunc(), criFsBinder_GetStatus(), criFsBinder_Unbind(), criFsBinder_BindCpk(), criFsBinder_BindFiles()
 */
CriError CRIAPI criFsBinder_BindDirectory(CriFsBinderHn bndrhn, CriFsBinderHn srcbndrhn, const CriChar8 *path, void *work, CriSint32 worksize, CriFsBindId *bndrid);

/**
 * \brief バインドIDの削除（アンバインド）：完了復帰関数
 * \ingroup FSLIB_BINDER
 * \param[in]	bndrid		バインドID
 * \return		CriError	エラーコード
 * \par 説明：
 * バインドIDをバインダーから削除します。<br>
 * 本関数は完了復帰関数です。
 * 指定されたバインドIDを削除できない場合、CRIERR_NGを返します。<br>
 * \par 補足：
 * 必要に応じてファイルのクローズ処理を行いますので、実行環境により数msecかかる場合があります。<br>
 * アンバインドするバインドIDに依存している他のバインドIDも同時にアンバインドされます（暗黙的アンバインド）。<br>
 * 例えば、CPKバインドIDのコンテンツファイルをFILEバインドしているバインドIDは、
 * 参照元のCPKバインドIDがアンバインドされると、暗黙的アンバンドされます。
 * 暗黙的にアンバインドされた項目は、暗黙的アンバインドリストに追加されます。<br>
 * 暗黙的アンバインドされたバインドIDは、通常どおりにcriFsBinder_Unbind関数でアンバインドするか、
 * criFsBinder_CleanImplicitUnbindList関数で暗黙的アンバインドリストをクリアする必要があります。
 * \par 例：
 * \code
 * // CPKファイルのバインド
 * criFsBinder_BindCpk(bndrhn, NULL, cpkpath, cpkwk, cpkwksz, &cpkid);
 * 	:
 * // fileidは、cpkidのコンテンツファイルをバインド
 * criFsBinder_BindFile(bndrhn, bndrhn, cntspath, filewk, filewksz, &fileid);
 * 	:
 * // CPKバインドIDのアンバインド
 * criFsBinder_Unbind(cpkid);	// ここでfileidは暗黙的アンバインドされる。
 * // FileバインドIDのアンバインド
 * criFsBinder_Unbind(fileid);
 * \endcode
 * \sa criFsBinder_BindCpk(), criFsBinder_BindFile(), criFsBinder_SetUserHeapFunc(), criFsBinder_CleanImplicitUnbindList()
 */
CriError CRIAPI criFsBinder_Unbind(CriFsBindId bndrid);

/**
 * \brief バインドIDの削除（アンバインド）：即時復帰関数
 * \ingroup FSLIB_BINDER
 * \param[in]	bndrid		バインドID
 * \return		CriError	エラーコード
 *
 * \par 説明：
 * バインドIDをバインダーから削除します。<br>
 * 本関数は即時復帰関数です。<br>
 * 指定されたバインドIDを削除できない場合、CRIERR_NGを返します。<br>
 * アンバインド処理を進行させるには、サーバー処理 criFsExecuteMain 関数と、criFsBinder_GetStatus 関数の
 * 呼び出しが必要です。<br>
 *
 * \par アンバインド完了の検知：
 * アンバインド処理の完了は criFsBinder_GetStatus 関数で取得されるステータスにより判断します。<br>
 * アンバインド処理中のステータスは CRIFSBINDER_STATUS_UNBIND になります。<br>
 * アンバインド完了後のステータスは CRIFSBINDER_STATUS_REMOVED となります。<br>
 *
 * \par 補足：
 * バインドIDとして取り得る値で、その時点において使用されていない値を指定した場合、取得されるステータスは
 * CRIFSBINDER_STATUS_REMOVEDになります。<br>
 * アンバインドするバインドIDに依存している他のバインドIDも同時にアンバインドされます（暗黙的アンバインド）。<br>
 * 例えば、CPKバインドIDのコンテンツファイルをFILEバインドしているバインドIDは、
 * 参照元のCPKバインドIDがアンバインドされると、暗黙的アンバンドされます。<br>
 * 暗黙的にアンバインドされた項目は、暗黙的アンバインドリストに追加されます。<br>
 * 暗黙的アンバインドされたバインドIDは、通常どおりにcriFsBinder_Unbind関数でアンバインドするか、
 * criFsBinder_CleanImplicitUnbindList関数で暗黙的アンバインドリストをクリアする必要があります。<br>
 *
 * \par 例：
 * \code
 * // CPKファイルのバインド
 * criFsBinder_BindCpk(bndrhn, NULL, cpkpath, cpkwk, cpkwksz, &bndrid);
 * 	:
 * // CPKバインドIDのアンバインド
 * criFsBinder_UnbindAsync(bndrid);
 * // アンバインド完了待ち
 * while (1) {
 *   CriFsBinderStatus status;
 *   criFsBinder_GetStatus(bndrid, &status);
 *   // アンバインド完了
 *   if (status==CRIFSBINDER_STATUS_REMOVED)
 *      break;
 *   // サーバー処理の実行
 *   criFs_ExecuteMain();
 * }
 * \endcode
 * \sa criFsBinder_BindCpk(), criFsBinder_BindFile(), criFsBinder_SetUserHeapFunc(), criFsBinder_CleanImplicitUnbindList()
 */
CriError CRIAPI criFsBinder_UnbindAsync(CriFsBindId bndrid);

/**
 * \brief 暗黙的アンバインドリストのクリア
 * \ingroup FSLIB_BINDER
 * \return	CriError	エラーコード
 * \par 説明：
 * 暗黙的アンバインドリストに登録されている全てのバインドIDを未使用リストへ戻します。<br>
 * 暗黙的アンバインドされるのバインドIDは、次の様な場合です。<br>
 *  ・他のバインドIDに依存したファイルをバインドしている場合（CPKのコンテンツファイルなど）。<br>
 *  ・親バインドIDがアンバインドされた場合。
 * \sa criFsBinder_Unbind()
 */
CriError CRIAPI criFsBinder_CleanImplicitUnbindList(void);

/**
 * \brief バインド状態の取得
 * \ingroup FSLIB_BINDER
 * \param[in]	bndrid	バインドID
 * \param[out]	status	CriFsBinderStatusバインダーステータス
 * \return	CriError	エラーコード
 * \par 説明：
 * 指定されたバインドIDのバインド状態を取得します。 <br>
 * バインド状態が CRIFSBINDER_STATUS_COMPLETE になるまでは、
 * そのバインドIDによるファイルアクセスを行えません。<br>
 * \par 例：
 * \code
 * CriFsBinderStatus status;
 * criFsBinder_GetStatus(bndrid, &status);
 * \endcode
 * \sa criFsBinder_BindCpk(), criFsBinder_BindFile(), criFsBinder_BindFiles()
 */
CriError CRIAPI criFsBinder_GetStatus(CriFsBindId bndrid, CriFsBinderStatus *status);

/**
 * \brief ファイル情報の取得（ファイル名指定）
 * \ingroup FSLIB_BINDER
 * \param[in]	bndrhn		バインダーハンドル
 * \param[in]	filepath	ファイルのフルパス
 * \param[out]	finfo		ファイル情報構造体
 * \param[out]	exist		ファイル検索結果（CRI_TRUE:取得成功　CRI_FALSE:取得失敗）
 * \return		CriError	エラーコード
 * \par 説明：
 * 指定されたファイルをバインダーから検索し、その情報を返します。<br>
 * バインド状態が CRIFSBINDER_STATUS_COMPLETE であるバインドIDのみを検索対象とします。<br>
 * ファイルが見付かった場合は existにCRI_TRUEを、ファイル情報構造体(finfo)にファイル情報をセットします。<br>
 * ファイルが見付からない場合、existにCRI_FALSEをセットします。<br>
 * finfoがNULLの場合、existにファイルの検索結果のみをセットします。
 * \par 例：
 * \code
 * CriFsBinderFileInfo finfo;
 * CriBool exist;
 * criFsBinder_Find(bndrhn, "a.txt", &finfo, &exist);
 * if (exist == CRI_TRUE) { // File is found.
 * }
 * else {// File cannot found.
 * }
 * \endcode
 * * \sa criFsBinder_GetStatus()
*/
CriError CRIAPI criFsBinder_Find(CriFsBinderHn bndrhn, const CriChar8 *filepath, CriFsBinderFileInfo *finfo, CriBool *exist);

/**
 * \brief ファイル情報の取得（ID指定）
 * \ingroup FSLIB_BINDER
 * \param[in]	bndrhn		バインダーハンドル
 * \param[in]	id			CPKコンテンツファイルID
 * \param[out]	finfo		ファイル情報構造体
 * \param[out]	exist		ファイル検索結果（CRI_TRUE:取得成功　CRI_FALSE:取得失敗）
 * \return		CriError 	エラーコード
 * \par 説明：
 * バインダーハンドルから、指定されたIDのファイルを検索し、その情報を返します。<br>
 * ID情報つきCPKファイルをバインドしている必要があります。<br>
 * バインド状態が CRIFSBINDER_STATUS_COMPLETE であるバインドIDのみを検索対象とします。<br>
 * ファイルが見付かった場合は existにCRI_TRUEをセットし、ファイル情報構造体(finfo)にファイル情報をセットします。<br>
 * ファイルが見付からない場合、existにCRI_FALSEをセットします。<br>
 * finfoがNULLの場合、existにファイルの検索結果(CRI_TRUE/CRI_FALSE)のみをセットします。
 * \par 例：
 * \code
 * CriFsBinderFileInfo finfo;
 * CriBool exist;
 * criFsBinder_FindById(bndrhn, 10, &finfo, &exist);
 * if (exist == CRI_TRUE) { // File is found.
 * }
 * else { // File cannot found.
 * }
 * \endcode
 * \sa criFsBinder_GetStatus() criFsBinder_BindCpk()
*/
CriError CRIAPI criFsBinder_FindById(CriFsBinderHn bndrhn, CriFsFileId id, CriFsBinderFileInfo *finfo, CriBool *exist);

/**
 * \brief CriFsBinderHnの取得
 * \ingroup FSLIB_BINDER
 * \param[in]	bndrid		バインドID
 * \param[out]	bndrhn		バインダーハンドル
 * \return		CriError	エラーコード
 * \par 説明：
 * CriFsBindIdをCriFsBinderHnに変換します。<br>
 * 新たにCriFsBinderHnを生成するものではなく、型変換を行うものと考えてください。 <br>
 * よって、実体としては同じものを指しており、本関数でCriFsBinderHnのリソースを消費することはありません。<br>
 * 元のCriFsBindIdもそのまま CriFsBindId として使用できます。<br>
 * バインドされたファイルの情報を得る場合、バインドされたバインドIDが順に検索されます。<br>
 * このため、特定のバインドIDにあるファイルにアクセスしたい場合、目的のバインドIDから
 * バインダーハンドルを取得して使用することで、効率的な検索を行うことが可能になります。<br>
 * \par 注意：
 * 本関数により取得された CriFsBinderHn は ::criFsBinder_Destroy 関数では破棄できません。
 * 元となる CriFsBindId を ::criFsBinder_Unbind 関数でアンバインドしてください。<br>
 * \par 例：
 * \code
 * // CPKをバインドする
 * criFsBinder_BindCpk(parent_bndrhn, NULL, cpkpath, work, worksize, &cpk_bndrid);
 * // バインドID からバインダーハンドルを得る
 * criFsBinder_GetHandle(cpk_bndrid, &cpk_bndrhn);
 * // このハンドルを用いてファイル情報を取得する
 * criFsBinder_Find(cpk_bndrhn, filepath, &finfo, &exist);
 *  :
 * // バインドIDをアンバインドする。取得されたバインダーハンドルも使用できなくなる
 * criFsBinder_Unbind(cpk_bndrid);
 * \endcode
 * \sa criFsBinder_Unbind
 */
CriError CRIAPI criFsBinder_GetHandle(CriFsBindId bndrid, CriFsBinderHn *bndrhn);

/**
 * \brief ファイルサイズの取得（ファイル名指定）
 * \ingroup FSLIB_BINDER
 * \param[in]	bndrhn		バインダーハンドル
 * \param[in]	filepath	ファイルのフルパス
 * \param[out]	size		ファイルのサイズ
 * \return		CriError	エラーコード
 * \par 説明：
 * 指定されたファイルのファイルサイズを取得します。<br>
 * まず、bndrhn のバインダーから目的のファイルを検索します。 <br>
 * bndrhn に目的のファイルが存在しない場合、デフォルトデバイスのfilepathのファイルを探します。
 * このとき、ファイルをオープン待ちが入る場合があります。<br>
 * バインド状態が CRIFSBINDER_STATUS_COMPLETE であるバインドIDが検索対象とします。<br>
 * 指定されたファイルが存在しない場合、sizeには負値が設定されます。
 * \par 備考:
 * 本関数は、内部的に ::criFsBinder_Find 関数を実行し、
 * ファイルがバインダーに登録されていなければファイルI/Oにアクセスしてファイルの有無を調べる、
 * という実装になっています。<br>
 * （バインダーに登録されていないファイルや、存在しないファイルに対して本関数を実行した場合、
 * ファイルI/Oへのアクセスが発生し、長時間処理がブロックされる可能性があります。）<br>
 * <br>
 * ファイルがバインダーに登録されているかどうかをチェックしたい場合には、
 * 本関数の代わりに、 ::criFsBinder_Find 関数をご利用ください。<br>
 * \attention
 * 以下のケースでは、本関数内で長時間処理がブロックされる場合があります。
 * 	- バインダーハンドルにNULLを指定した場合
 * 	- バインダーに登録されていないファイルを指定した場合
 * 	- 無効なパス（存在しないファイル）を指定した場合
 * 	- criFsBinder_BindDirectory関数でディレクトリをバインドしたハンドルを指定した場合
 * \sa criFsBinder_GetFileSizeById, criFsBinder_Find
 */
CriError CRIAPI criFsBinder_GetFileSize(CriFsBinderHn bndrhn, const CriChar8 *filepath, CriSint64 *size);

/**
 * \brief ファイルサイズの取得（ID指定）
 * \ingroup FSLIB_BINDER
 * \param[in]	bndrhn		バインダーハンドル
 * \param[in]	id			ファイルのID
 * \param[out]	size		ファイルのサイズ
 * \return		CriError	エラーコード
 * \par 説明：
 * ファイルサイズを取得します。<br>
 * ID情報つきCPKファイルがバインドされている必要があります。<br>
 * bndrhn のバインダーから目的のファイルを検索します。
 * バインド状態が CRIFSBINDER_STATUS_COMPLETE であるCPKバインドIDのみを検索対象とします。<br>
 * \sa criFsBinder_GetFileSize
 */
CriError CRIAPI criFsBinder_GetFileSizeById(CriFsBinderHn bndrhn, CriFsFileId id, CriSint64 *size);

/**
 * \brief ROMアドレスの取得（ファイル名指定）
 * \ingroup FSLIB_BINDER_EMB
 * \param[in]	bndrhn		バインダーハンドル
 * \param[in]	filepath	ファイルのフルパス
 * \param[out]	rom_address	ROMアドレス
 * \return		CriError	エラーコード
 * \par 説明：
 * 指定されたファイルのROMアドレスを取得します。<br>
 * bndrhn のバインダーから目的のファイルを検索します。 <br>
 * バインド状態が CRIFSBINDER_STATUS_COMPLETE であるバインドIDが検索対象とします。<br>
 * バインダーの使用は必須です。バインダーハンドルにはNULLは指定できません。
 * ディレクトリバインドはサポートしません。criFsBinder_BindDirectory で使用したハンドルは指定できません。
 * 指定されたファイルが存在しない場合、rom_address には CRIFS_ROM_ADDRESS_INVALID が設定されます。
 * \attention
 * ゲーム機向けでは非サポートです。この関数は呼び出さないでください。<br>
 * \sa criFsBinder_GetRomAddress() criFsBinder_GetRomAddressById()
 */
CriError CRIAPI criFsBinder_GetRomAddress(CriFsBinderHn bndrhn, const CriChar8 *filepath, CriUint64Adr *rom_address);

/**
 * \brief ROMアドレスの取得（ID指定）
 * \ingroup FSLIB_BINDER_EMB
 * \param[in]	bndrhn		バインダーハンドル
 * \param[in]	id			ファイルのID
 * \param[out]	rom_address	ROMアドレス
 * \return		CriError	エラーコード
 * \par 説明：
 * 指定されたファイルのROMアドレスを取得します。<br>
 * ID情報つきCPKファイルがバインドされている必要があります。<br>
 * bndrhn のバインダーから目的のファイルを検索します。
 * バインド状態が CRIFSBINDER_STATUS_COMPLETE であるCPKバインドIDのみを検索対象とします。<br>
 * 指定されたファイルIDが存在しない場合、rom_address には CRIFS_ROM_ADDRESS_INVALID が設定されます。
 * \attention
 * ゲーム機向けでは非サポートです。この関数は呼び出さないでください。<br>
 * \sa criFsBinder_GetRomAddress() criFsBinder_GetRomAddressById()
 */
CriError CRIAPI criFsBinder_GetRomAddressById(CriFsBinderHn bndrhn, CriFsFileId id, CriUint64Adr *rom_address);

/**
 * \brief プライオリティ値の取得
 * \ingroup FSLIB_BINDER
 * \param[in]	bndrid		バインドID
 * \param[out]	priority	プライオリティ値
 * \return		CriError	エラーコード
 * \par 説明：
 * バインドIDのプライオリティ値を取得します。<br>
 * プライオリティにより、バインダーハンドル内における、バインドIDの検索順を制御できます。<br>
 * バインド時のプライオリティ値は０です。<br>
 * 同プライオリティ内の検索順は、後に ::criFsBinder_SetPriority が指定された順番になります。
 * ::criFsBinder_SetPriority が呼び出されていない場合、先にバインドされた順番になります。
 * プライオリティ値の大きい方が高プライオリティとなり、検索順が高くなります。
 * \sa criFsBinder_SetPriority()
 */
CriError CRIAPI criFsBinder_GetPriority(CriFsBindId bndrid, CriSint32 *priority);

/**
 * \brief プライオリティ値の設定
 * \ingroup FSLIB_BINDER
 * \param[in]	bndrid		バインドID
 * \param[in]	priority	プライオリティ値
 * \return		CriError	エラーコード
 * \par 説明：
 * バインドIDにプライオリティを設定します。 <br>
 * プライオリティにより、バインダーハンドル内における、バインドIDの検索順を制御できます。<br>
 * バインド時のプライオリティ値は０です。<br>
 * 同プライオリティ内の検索順は、後に ::criFsBinder_SetPriority が指定された順番になります。
 * ::criFsBinder_SetPriority が呼び出されていない場合、先にバインドされた順番になります。
 * プライオリティ値の大きい方が高プライオリティとなり、検索順が高くなります。
 * \par 例：
 * \code
 * // a.cpk(a_id), b.cpk(b_id) の順にバインド
 * criFsBinder_BindCpk(bndrhn, NULL, "a.cpk", a_wk, a_wksz, a_id);
 * criFsBinder_BindCpk(bndrhn, NULL, "b.cpk", a_wk, a_wksz, b_id);
 * // この時点では a_id, b_idの順に検索される。
 * 	:
 * criFsBinder_SetPriority(b_id, 1);
 * // b_id のプライオリティをデフォルト値よりも上げたので、
 * // 検索順は b_id, a_id となる。
 * \endcode
 * \sa criFsBinder_GetPriority()
*/
CriError CRIAPI criFsBinder_SetPriority(CriFsBindId bndrid, CriSint32 priority);

/**
 * \brief カレントディレクトリの設定
 * \ingroup FSLIB_BINDER
 * \param[in]	bndrId		バインドID
 * \param[in]	path		カレントディレクトリ
 * \param[in]	work		カレントディレクトリ名保存用ワーク領域
 * \param[in]	worksize	カレントディレクトリ名保存用ワーク領域サイズ
 * \return		CriError	エラーコード
 * \par 説明：
 * バインドIDにカレントディレクトリを設定します。<br>
 * 必要なワーク領域が確保できない場合、カレントディレクトリの設定に失敗します。<br>
 * この場合、既に設定されているカレントディレクトリ設定は破棄されます。<br>
 * バインドIDを利用してファイルを参照する際に、カレントディレクトリがパス名の前に付加されます。<br>
 * 指定するワーク領域のサイズは、設定するカレントディレクトリ名を格納するために使用されます。<br>
 * 最低でも、strlen(path)+1 バイトの領域を確保して渡してください。
 * メモリ確保／解放コールバック関数が登録されている場合、ワーク領域にNULL(ワークサイズは０）を設定すると、
 * 必要なワーク領域をメモリ確保／解放コールバック関数を使用して動的に確保します。<br>
 * \par 例：
 * \code
 * // バインド直後は、カレントディレクトリは設定されていない。
 * criFsBinder_BindCpk(bndrhn, NULL, "cpk.cpk", wk, wksz, bndrid);
 * 	:
 * criFsBinder_Find(bndrhn, "a.txt", NULL, &exist);　// "a.txt" で検索される
 * 	:
 * // カレントディレクトリ"/folder/"を設定
 * worksz = strlen("/folder/")+1;
 * work = malloc(worksz);
 * criFsBinder_SetCurrentDirectory(bndrid, "/folder/", work, worksz);
 * criFsBinder_Find(bndrhn, "a.txt", NULL, &exist);　// "/folder/a.txt" で検索される
 * \endcode
 * \sa criFsBinder_SetUserHeapFunc()
 */
CriError CRIAPI criFsBinder_SetCurrentDirectory(CriFsBindId bndrId, const CriChar8 *path, void *work, CriSint32 worksize);

/**
 * \brief CPKコンテンツファイル情報の取得
 * \ingroup FSLIB_BINDER
 * \param[in]	bndrhn		バインダーハンドル
 * \param[in]	path	    コンテンツファイルパス名
 * \param[out]	cfinf		CriFsBinderContentsFileInfo構造体へのポインター
 * \return		CriError	エラーコード
 * \par 説明：
 * ファイル名情報付きCPKファイルから、指定されたコンテンツファイル名のファイル情報を取得します。<br>
 * 指定されたコンテンツファイルを格納しているCPKが、ファイル名情報付CPKである必要があります。<br>
 * 指定したバインダーハンドルに、同じ名前のファイルが複数存在する場合、最初に見付けたファイルを
 * 格納しているCPKが選択されます。<br>
 * 特定のCPKファイルを直接指定したい場合、criFsBinder_GetHandle関数により、そのCPKのバインダーIDから
 * バインダーハンドルを取得し、本関数の引数としてください。<br>
 * \par 注意：
 * 本機能を使用するには、『CPK File Builder Ver.1.03以降』を使用して作成した、ファイル名情報付CPKを
 * 用いる必要があります。
 * \par 例：
 * \code
 * CriFsBinderContentsFileInfo cfinf;
 * // CPKをバインド
 * criFsBinder_BindCpk(parent_bndrhn, NULL, cpkpath, work, worksize, &cpk_bndrid);
 * // コンテンツファイルの情報を取得
 * criFsBinder_GetContentsFileInfo(parent_bndrhn, CPK_CONTENTS_FILENAME, &cfinf);
 * \endcode
 */
CriError criFsBinder_GetContentsFileInfo(CriFsBinderHn bndrhn, const CriChar8 *path, CriFsBinderContentsFileInfo *cfinf);

/**
 * \brief CPKコンテンツファイル情報の取得
 * \ingroup FSLIB_BINDER
 * \param[in]	bndrhn		バインダーハンドル
 * \param[in]	id			ファイルID
 * \param[out]	cfinf		CriFsBinderContentsFileInfo構造体へのポインター
 * \return		CriError	エラーコード
 * \par 説明：
 * ID+ファイル名情報付きCPKファイルから、指定されたファイルIDのファイル情報を取得します。<br>
 * 指定されたファイルを格納しているCPKが、ID+ファイル名情報付CPKである必要があります。<br>
 * 指定したバインダーハンドルに、同じIDのファイルが複数存在する場合、最初に見付けたファイルを
 * 格納しているCPKが選択されます。<br>
 * 特定のCPKファイルを直接指定したい場合、criFsBinder_GetHandle関数により、そのCPKのバインドIDから
 * バインダーハンドルを取得し、本関数の引数としてください。<br>
 * \par 注意：
 * 本機能を使用するには、『CPK File Builder Ver.1.03以降』を使用して作成した、ID+ファイル名情報付CPKを
 * 用いる必要があります。
 * \par 例：
 * \code
 * CriFsBinderContentsFileInfo cfinf;
 * // CPKをバインド
 * criFsBinder_BindCpk(parent_bndrhn, NULL, cpkpath, work, worksize, &cpk_bndrid);
 * // ID 0x00000010 のファイル情報を取得
 * criFsBinder_GetContentsFileInfoById(parent_bndrhn, 0x00000010, &cfinf);
 * \endcode
 */
CriError CRIAPI criFsBinder_GetContentsFileInfoById(CriFsBinderHn bndrhn, CriFsFileId id, CriFsBinderContentsFileInfo *cfinf);

/**
 * \brief Index指定によるCPKコンテンツファイル情報の取得
 * \ingroup FSLIB_BINDER
 * \param[in]	bndrid		バインドID
 * \param[in]	index		情報を取得するコンテンツファイルの先頭INDEX
 * \param[out]	cfinf		CriFsBinderContentsFileInfo構造体配列
 * \param[in]   n           情報を取得するコンテンツファイルの数
 * \return		CriError	エラーコード
 * \par 説明：
 * CPKファイルのコンテンツファイル、indexからn個分のファイル情報を取得します。<br>
 * indexはCPK作成時、コンテンツファイルに対して０から割付られます。<br>
 * index, nの上限値はcriFsBinder_GetBinderIdInfo関数で取得されるファイル数となります。
 * IDのみのモードで作成されたCPKは、「ディレクトリ名」、「ファイル名」、「ユーザー文字列」の情報を持たないため、
 * CriFsBinderContentsFileInfo構造体配列の該当メンバには NULL が格納されます。<br>
 * \par 例：CPKの全コンテンツファイル情報を取得する
 * \code
 * CriFsBinderInfo binf;
 * CriFsBinderContentsFileInfo *cfinf;
 * // CPKをバインド
 * criFsBinder_BindCpk(parent_bndrhn, NULL, cpkpath, work, worksize, &cpk_bndrid);
 *  : // CPKバインド完了を待つ。
 *  :
 * // CPKのコンテンツファイル数を取得する。
 * criFsBinder_GetBinderIdInfo(cpk_bndrid, &binf);
 * // コンテンツファイル数全ての情報を取得するための情報格納領域を確保する。
 * cfinf = malloc(sizeof(CriFsBinderContentsFileInfo)*binf.nfiles);
 * // CPKの全コンテンツファイルの情報を取得する
 * criFsBinder_GetContentsFileInfoByIndex(cpk_bndrid, 0, cfinf, binf.nfiles);
 * \endcode
 * \par 例：CPKの一部コンテンツファイル（Index5番から10ファイル分）の情報を取得する
 * \code
 * #define CNTNTS_INDX (5)  // INDEX 5から
 * #define CNTNTS_N (10)  // 10コンテンツファイル
 * CriFsBinderInfo binf;
 * CriFsBinderContentsFileInfo *cfinf;
 * // CPKをバインド
 * criFsBinder_BindCpk(parent_bndrhn, NULL, cpkpath, work, worksize, &cpk_bndrid);
 *  : // CPKバインド完了を待つ。
 *  :
 * // 取得するコンテンツファイル数分の情報を取得するための情報格納領域を確保する。
 * cfinf = malloc(sizeof(CriFsBinderContentsFileInfo)*CNTNTS_N);
 * // CPKの全コンテンツファイルの情報を取得する
 * criFsBinder_GetContentsFileInfoByIndex(cpk_bndrid, CNTNTS_INDX, cfinf, CNTNTS_N);
 * \endcode
 * \par 備考：
 * 情報取得に失敗した場合であっても、本関数はエラー値を返さない場合があります。<br>
 * ただし、その場合であっても、コンテンツファイル情報のファイル名がNULLでかつ、IDが-1になっている事から、
 * 情報取得に失敗した事を判定可能です。<br>
 */
CriError CRIAPI criFsBinder_GetContentsFileInfoByIndex(CriFsBindId bndrid, CriSint32 index, CriFsBinderContentsFileInfo *cfinf, CriSint32 n);

/**
 * \brief CPKコンテンツファイル情報に含まれるユーザー文字列の取得
 * \ingroup FSLIB_BINDER
 * \param[in]	bndrhn		バインダーハンドル
 * \param[in]	path	    コンテンツファイルパス名
 * \param[out]	ustr		ユーザー文字列格納領域
 * \param[in]	length		ユーザー文字列格納領域サイズ（単位はバイト）
 * \return		CriError	エラーコード
 * \par 説明：
 * ファイル名情報付きCPKファイルから、指定されたコンテンツファイル名のファイル情報に含まれるユーザー文字列を取得します。<br>
 * 指定されたコンテンツファイルを格納しているCPKが、ファイル名情報付CPKである必要があり、<br>
 * なおかつファイル情報にユーザー文字列が付与されている必要があります。<br>
 * ユーザー文字列を付与するためには、CPK File Builder (GUI ツール) ならば「ファイル情報にアトリビュート文字列を埋め込む」を有効に、<br>
 * CRI Packed File Makerコンソール版ならば「-attrstring」オプションを指定してCPKを作成してください。<br>
 * 詳細な作成方法はツールのマニュアルを参照してください。<br>
 * CPKが上記の設定で作成されていない場合、本関数は失敗し、エラーを返します。<br>
 * この場合、ustr には NULL が格納されます。<br>
 * CPKは上記の設定で作成されたが、アトリビュートが指定されていないコンテンツファイルが本関数に指定された場合、<br>
 * エラーは発生せずに ustr には "\0" が格納されます。<br>
 * 指定したバインダーハンドルに、同じ名前のファイルが複数存在する場合、最初に見付けたファイルを
 * 格納しているCPKが選択されます。<br>
 * 特定のCPKファイルを直接指定したい場合、criFsBinder_GetHandle関数により、そのCPKのバインダーIDから
 * バインダーハンドルを取得し、本関数の引数としてください。<br>
 * \par 注意：
 * 本機能を使用するには、『CPK File Builder Ver.1.03以降』を使用して作成した、ファイル名情報付CPKを
 * 用いる必要があります。
 * \par 注意：
 * ユーザー文字列の長さの制約は CRI File System ライブラリでは設けておりません。<br>
 * ユーザー側でユーザー文字列の最大長を規定し、十分な格納領域を確保してください。<br>
 * CPKツールのアトリビュート文字列を埋め込む機能でユーザー文字列を指定した場合、<br>
 * "CRI_CFATTR:" という接頭辞が付与されますので、その分のサイズも考慮する必要があります。<br>
 * ユーザー文字列の終端まで収まらないほど格納領域サイズが小さい場合は、格納領域の末尾を終端文字に置換し、警告を返します。<br>
 * \par 例：
 * \code
 * CriChar8 ustr[256];
 * // CPKをバインド
 * criFsBinder_BindCpk(parent_bndrhn, NULL, cpkpath, work, worksize, &cpk_bndrid);
 * // コンテンツファイルの情報に含まれるユーザー文字列を取得
 * criFsBinder_GetContentsFileUserString(parent_bndrhn, CPK_CONTENTS_FILENAME, &ustr, 256);
 * \endcode
 */
CriError criFsBinder_GetContentsFileUserString(CriFsBinderHn bndrhn, const CriChar8 *path, CriChar8 *ustr, CriSint32 length);

/**
 * \brief CPKコンテンツファイル情報に含まれるユーザー文字列の取得 (ID指定)
 * \ingroup FSLIB_BINDER
 * \param[in]	bndrhn		バインダーハンドル
 * \param[in]	id			ファイルID
 * \param[out]	ustr		ユーザー文字列格納領域
 * \param[in]	length		ユーザー文字列格納領域サイズ（単位はバイト）
 * \return		CriError	エラーコード
 * \par 説明：
 * ID+ファイル名情報付きCPKファイルから、指定されたファイルIDのファイル情報に含まれるユーザー文字列を取得します。<br>
 * 指定されたファイルを格納しているCPKが、ID+ファイル名情報付CPKである必要があり、<br>
 * なおかつファイル情報のユーザー文字列が付与されている必要があります。<br>
 * ユーザー文字列を付与するためには、CPK File Builder (GUI ツール) ならば「ファイル情報にアトリビュート文字列を埋め込む」を有効に、<br>
 * CRI Packed File Makerコンソール版ならば「-attrstring」オプションを指定してCPKを作成してください。<br>
 * 詳細な作成方法はツールのマニュアルを参照してください。<br>
 * CPKが上記の設定で作成されていない場合、本関数は失敗し、エラーを返します。<br>
 * この場合、ustr には NULL が格納されます。<br>
 * CPKは上記の設定で作成されたが、アトリビュートが指定されていないコンテンツファイルが本関数に指定された場合、<br>
 * エラーは発生せずに ustr には "\0" が格納されます。<br>
 * 指定したバインダーハンドルに、同じIDのファイルが複数存在する場合、最初に見付けたファイルを
 * 格納しているCPKが選択されます。<br>
 * 特定のCPKファイルを直接指定したい場合、criFsBinder_GetHandle関数により、そのCPKのバインドIDから
 * バインダーハンドルを取得し、本関数の引数としてください。<br>
 * \par 注意：
 * 本機能を使用するには、『CPK File Builder Ver.1.03以降』を使用して作成した、ID+ファイル名情報付CPKを
 * 用いる必要があります。
 * \par 注意：
 * ユーザー文字列の長さの制約は CRI File System ライブラリでは設けておりません。<br>
 * ユーザー側でユーザー文字列の最大長を規定し、十分な格納領域を確保してください。<br>
 * CPKツールのアトリビュート文字列を埋め込む機能でユーザー文字列を指定した場合、<br>
 * "CRI_CFATTR:" という接頭辞が付与されますので、その分のサイズも考慮する必要があります。<br>
 * ユーザー文字列の終端まで収まらないほど格納領域サイズが小さい場合は、格納領域の末尾を終端文字に置換し、警告を返します。<br>
 * \par 例：
 * \code
 * CriChar8 ustr[256];
 * // CPKをバインド
 * criFsBinder_BindCpk(parent_bndrhn, NULL, cpkpath, work, worksize, &cpk_bndrid);
 * // ID 0x00000010 のファイル情報に含まれるユーザー文字列を取得
 * criFsBinder_GetContentsFileUserStringById(parent_bndrhn, 0x00000010, &ustr, 256);
 * \endcode
 */
CriError CRIAPI criFsBinder_GetContentsFileUserStringById(CriFsBinderHn bndrhn, CriFsFileId id, CriChar8 *ustr, CriSint32 length);

/**
 * \brief バインドID情報の取得
 * \ingroup FSLIB_BINDER
 * \param[in]	bndrid		バインドID
 * \param[out]	binf		取得情報
 * \return		CriError	エラーコード
 * \par 説明：
 * 指定されたバインドIDのバインダー種別（CPK/ファイル/ディレクトリ等）やバインドしたファイル名、プライオリティ設定などの
 * 情報を取得します。<br>
 */
CriError CRIAPI criFsBinder_GetBinderIdInfo(CriFsBindId bndrid, CriFsBinderInfo *binf);

/**
 * \brief グループファイル数の取得
 * \ingroup FSLIB_BINDER
 * \param[in]	bndrid		バインドID
 * \param[in]	groupname	グループ名
 * \param[in]	attrname	アトリビュート名
 * \param[out]	groupfiles	グループファイル数
 * \return		CriError	エラーコード
 * \par 説明：
 * 指定したバインドID、グループ名、アトリビュート名に適合するファイルの数を取得します。<br>
 * 適合するファイルが存在しない場合、ファイル数は０になります。<br>
 * グループ情報付きのCPKファイルをバインドしたバインドIDを指定する必要があります。<br>
 * 無効なバインドIDを指定した場合、エラーコールバックが起きます。<br>
 * アトリビュート名にNULLを指定した場合、指定グループに属する全てのファイルがグループロードの対象となります。<br>
 * また、パッキングツールのアトリビュート指定を「none」とした場合も、アトリビュート名にNULLを指定してください。<br>
 */
CriError CRIAPI criFsBinder_GetNumberOfGroupFiles(CriFsBindId bndrid, const CriChar8 *groupname, const CriChar8 *attrname, CriSint32 *groupfiles);

/**
 * \brief グループロードサイズの取得
 * \ingroup FSLIB_BINDER
 * \param[in]	bndrid		バインドID
 * \param[in]	groupname	グループ名
 * \param[in]	attrname	アトリビュート名
 * \param[out]	datasize    グループロードサイズ
 * \return		CriError	エラーコード
 * \par 説明：
 * 指定したバインドID、グループ名、アトリビュート名に適合するグループのロードに必要な読込領域のサイズを取得します。<br>
 * アライメントなども加味されたデータサイズとなります。<br>
 * 適合するファイルが存在しない場合、グループロードサイズは０になります。<br>
 * グループ情報付きのCPKファイルをバインドしたバインドIDを指定する必要があります。<br>
 * 無効なバインドIDを指定した場合、エラーコールバックが起きます。<br>
 * アトリビュート名にNULLを指定した場合、指定グループに属する全てのファイルがグループロードの対象となります。<br>
 * また、パッキングツールのアトリビュート指定を「none」とした場合も、アトリビュート名にNULLを指定してください。<br>
 */
CriError CRIAPI criFsBinder_GetTotalGroupDataSize(CriFsBindId bndrid, const CriChar8 *groupname, const CriChar8 *attrname, CriSint64 *datasize);

/**
 * \brief ID情報付CPKのアクセス情報テーブル作成用ワークサイズ取得
 * \ingroup FSLIB_BINDER
 * \param[in]	bindrid	バインドID
 * \param[in]	steps		アクセス情報テーブル作成の作成間隔
 * \param[out]	worksize 	アクセス情報テーブル作成用ワーク領域のサイズ
 * \return		CriError	エラーコード
 * \par 説明：
 * ID情報付CPKのアクセス情報テーブルの作成に必要なワーク領域のサイズを取得します。<br>
 * アクセス情報テーブルとは、CPKのコンテンツファイルにアクセスする際の処理を前もって行うことで、
 * アクセス速度を向上させるためのものです。<br>
 * ID情報のないCPKではアクセステーブルは必要ありません。
 * 本関数はID情報付CPKに対してのみ有効です。<br>
 * 本関数は完了復帰関数です。<br>
 * アクセス情報テーブルの領域は、該当CPKがバインドされている間は解放したり、他へ転用しないでください。<br>
 * \par アクセス情報テーブルの作成間隔について
 * stepsに１を指定した場合、全てのコンテンツファイルに対するアクセス情報テーブルが作成されます。
 * この場合、アクセス情報テーブルの要素数はコンテンツファイル数と同じになります。<br>
 * stepsに１より大きい値を指定した場合は、指定stepsおきのコンテンツファイルに対するアクセス情報テーブルが作成されます。
 * アクセス情報テーブルの要素数はstepsに応じて変化します。<br>
 * 全てのコンテンツファイルに対するテーブルを準備しないので、stepsに1を設定したときに比べて、
 * ファイルアクセス時の処理がかかりますが、アクセス情報テーブルの領域を小さくできます。
 */
CriError CRIAPI criFsBinder_GetWorkSizeForCpkIdAccessTable(CriFsBindId bindrid, CriSint32 steps, CriSint32 *worksize);

/**
 * \brief ID情報付CPK アクセス情報テーブルの作成
 * \ingroup FSLIB_BINDER
 * \param[in]	binderid	バインドID
 * \param[in]	steps		アクセス情報テーブル作成の作成間隔
 * \param[in]	work		アクセス情報テーブル作成用ワーク領域
 * \param[in]	worksize 	アクセス情報テーブル作成用ワーク領域のサイズ
 * \return		CriError	エラーコード
 * \par 説明：
 * ID情報付CPKのアクセス情報テーブルを作成します。<br>
 * アクセス情報テーブルを作成した場合、該当CPKのコンテンツへのアクセスが高速になります。
 * 本関数は完了復帰関数です。コンテンツファイル数が多い場合、復帰までに時間がかかる場合があります。<br>
 * ID情報のないCPKではアクセステーブルは作成されません。<br>
 * 本関数はID情報付CPKに対してのみ有効です。<br>
 * アクセス情報テーブルの領域は、該当CPKがバインドされている間は解放したり、他へ転用しないでください。<br>
 * アクセス情報テーブルの領域は、該当CPKをアンバインドした後に解放してください。<br>
 */
CriError CRIAPI criFsBinder_SetupCpkIdAccessTable(CriFsBindId binderid, CriSint32 steps, void *work, CriSint32 worksize);

/**
 * \brief バインド中のファイルの一時クローズ
 * \ingroup FSLIB_BINDER
 * \param[in]	bind_id					バインドID
 * \param[out]	internal_loader_status	内部ローダーステータス
 * \return		CriError				エラーコード
 * \par 説明：
 * バインド中のファイルを一旦クローズします。<br>
 * ::criFsBinder_Unbind 関数と異なり、
 * CPKファイルのTOC情報をメモリ上に残したままファイルのみをクローズします。<br>
 * （バインドIDに紐付けられているネイティブファイルのハンドルがクローズされます。）<br>
 * \par 備考:
 * 本関数は、CPKファイルはバインドした状態にしておきたいが、
 * 他のファイルをオープンするため、一時的にファイルハンドルを解放したい場合等に使用します。<br>
 * <b>※ファイルハンドル等のハードウェアリソースがプラットフォームで利用可能な上限数に達しない限り、
 * 本関数を使用する必要はありません。</b><br>
 * <br>
 * 本関数は同期APIです。<br>
 * 本関数を実行すると、ファイルのオープン完了まで処理がブロックされます。<br>
 * オープン処理を非同期に行いたい場合には、本関数の代わりに
 * ::criFsBinder_CloseFileAsync 関数をご使用ください。<br>
 * \attention
 * 本関数でクローズしたCPKファイルに対してアクセスすると、リードエラーが発生します。<br>
 * 本関数実行後、 ::criFsBinder_ReopenFile 関数を実行するまでの間、
 * 当該CPKファイルに対するアクセスが発生しないよう制御する必要があります。<br>
 * <br>
 * 本関数は、CPKファイルを直接バインドしているケースでのみ利用可能です。<br>
 * ディレクトリバインドを使用しているケースや、ファイルバインドを使用しているケース、
 * CPKファイル内のコンテンツをバインドしているケースでは、本関数は使用できません。<br>
 * \sa criFsBinder_ReopenFile
 */
CriError CRIAPI criFsBinder_CloseFile(
	CriFsBindId bind_id, CriFsLoaderStatus *internal_loader_status);

/**
 * \brief 一時クローズファイルの再オープン
 * \ingroup FSLIB_BINDER
 * \param[in]	bind_id					バインドID
 * \param[out]	internal_loader_status	内部ローダーステータス
 * \return		CriError				エラーコード
 * \par 説明：
 * ::criFsBinder_CloseFile 関数等でクローズしたファイルを再度オープンし直します。<br>
 * \par 備考:
 * 本関数は同期APIです。<br>
 * 本関数を実行すると、ファイルのオープン完了まで処理がブロックされます。<br>
 * オープン処理を非同期に行いたい場合には、本関数の代わりに
 * ::criFsBinder_ReopenFileAsync 関数をご使用ください。<br>
 * \attention
 * リードエラー等によりファイルの再オープンに失敗した場合、
 * 第 2 引数（ internal_loader_status ）が ::CRIFSLOADER_STATUS_ERROR になります。<br>
 * \sa criFsBinder_CloseFile
 */
CriError CRIAPI criFsBinder_ReopenFile(
	CriFsBindId bind_id, CriFsLoaderStatus *internal_loader_status);

/**
 * \brief バインド中のファイルの一時クローズ
 * \ingroup FSLIB_BINDER
 * \param[in]	bind_id				バインドID
 * \param[out]	internal_loader		内部ローダーハンドル
 * \return		CriError			エラーコード
 * \par 説明：
 * バインド中のファイルを一旦クローズします。<br>
 * 処理が非同期な点を除き、 ::criFsBinder_CloseFile 関数と同様の操作を行います。<br>
 * \par 備考：
 * 本関数は非同期処理用のAPIです。<br>
 * 本関数を実行すると、ファイルのクローズ完了を待つことなく処理が復帰します。<br>
 * <br>
 * ファイルのクローズが完了したかどうかは、第 2 引数（ internal_loader ）
 * で返されるローダーハンドルを使用してチェックします。<br>
 * （クローズ処理が完了したタイミングで、ローダーハンドルのステータスが
 * ::CRIFSLOADER_STATUS_COMPLETE に遷移します。）<br>
 * \attention
 * 第 2 引数（ internal_loader ）で返されるローダーハンドルは、
 * ライブラリ内でファイルアクセスのために使用される内部リソースです。<br>
 * このハンドルをアプリケーション中で破棄しないでください。<br>
 * \sa criFsBinder_CloseFile
 */
CriError CRIAPI criFsBinder_CloseFileAsync(
	CriFsBindId bind_id, CriFsLoaderHn *internal_loader);

/**
 * \brief 一時クローズファイルの再オープン
 * \ingroup FSLIB_BINDER
 * \param[in]	bind_id				バインドID
 * \param[out]	internal_loader		内部ローダーハンドル
 * \return		CriError			エラーコード
 * \par 説明：
 * ::criFsBinder_CloseFile 関数等でクローズしたファイルを再度オープンし直します。<br>
 * 処理が非同期な点を除き、 ::criFsBinder_ReopenFile 関数と同様の操作を行います。<br>
 * \par 備考：
 * 本関数は非同期処理用のAPIです。<br>
 * 本関数を実行すると、ファイルのオープン完了を待つことなく処理が復帰します。<br>
 * <br>
 * ファイルのオープンが完了したかどうかは、第 2 引数（ internal_loader ）
 * で返されるローダーハンドルを使用してチェックします。<br>
 * （オープン処理が完了したタイミングで、ローダーハンドルのステータスが
 * ::CRIFSLOADER_STATUS_COMPLETE に遷移します。）<br>
 * ファイルオープンの完了を確認した後、::criFsBinder_CompleteAsyncFileReopen 関数を使い、<br>
 * バインドIDに対してファイルの再オープンを完了してください。
 * \attention
 * 第 2 引数（ internal_loader ）で返されるローダーハンドルは、
 * ライブラリ内でファイルアクセスのために使用される内部リソースです。<br>
 * このハンドルをアプリケーション中で破棄しないでください。<br>
 * <br>
 * リードエラー等によりファイルの再オープンに失敗した場合、
 * 第 2 引数（ internal_loader_status ）で返されたローダーハンドルのステータスが
 * ::CRIFSLOADER_STATUS_ERROR になります。<br>
 * \sa criFsBinder_ReopenFile
 */
CriError CRIAPI criFsBinder_ReopenFileAsync(
	CriFsBindId bind_id, CriFsLoaderHn *internal_loader);

/**
 * \brief 一時クローズファイルの再オープン反映
 * \ingroup FSLIB_BINDER
 * \param[in]	bind_id				バインドID
 * \return		CriError			エラーコード
 * \par 説明：
 * ::criFsBinder_ReopenFileAsync 関数によるファイルの再オープン情報を、<br>
 * バインドIDに反映させ、再オープン処理を完了します。<br>
 * ::criFsBinder_ReopenFileAsync 関数の実行で取得したローダーハンドルのステータスが<br>
 * ::CRIFSLOADER_STATUS_COMPLETE に遷移した事を確認してから実行してください。<br>
 * ファイルを再オープンしたバインドIDに対して、<br>
 * 本関数を実行せずにロードを実行すると、ロードに失敗します。
 * \par
 * ::criFsBinder_ReopenFile 関数に対しては本関数を実行する必要はありません。<br>
 * \attention
 * ファイル再オープン用ローダーハンドルのステータスが<br>
 * ::CRIFSLOADER_STATUS_COMPLETE 以外の状態で本関数を実行すると、<br>
 * 本関数はエラーを返します。<br>
 */
CriError CRIAPI criFsBinder_CompleteAsyncFileReopen(CriFsBindId bind_id);

/**
 * \brief 複数ファイルバインド用パスセパレータの指定
 * \ingroup FSLIB_BINDER
 * \param[in]	filter				セパレータとして使用する文字の一覧
 * \return		CriError			エラーコード
 * \par 説明：
 * ::criFsBinder_BindFiles 関数がセパレータとして解釈する文字を変更します。<br>
 * <br>
 * ::criFsBinder_BindFiles 関数を使って複数のファイルをバインドする場合、
 * ファイルパスをセパレータで結合した文字列を指定する必要があります。<br>
 * デフォルト状態では、CRI File Systemライブラリは「,」「\\t」「\\n」
 * の3つをセパレータとして扱います。<br>
 * セパレータに上記以外の文字を使用したい場合、
 * 本関数でセパレータに該当する文字を変更することが可能です。<br>
 * <br>
 * 第一引数の filter には、
 * セパレータとして使用する文字の一覧を格納した文字列を指定します。<br>
 * 例えば、 "@+-*" を指定した場合、
 * 「@」「+」「-」「*」の4種類の文字がセパレータとして扱われます。<br>
 * \par 備考：
 * filter に空文字列（""）を指定した場合、セパレータが無効になります。<br>
 * filter に NULL を指定した場合、設定がデフォルト状態に戻ります。<br>
 * \attention
 * ::criFsBinder_BindFiles 関数以外の関数にセパレータを含むパスを渡した場合、
 * CRI File Systemライブラリは不正なパスとして処理されます。<br>
 * （セパレータ以降のパスが無効になります。）<br>
 * ::criFsBinder_BindFiles 関数を使用しない場合でも、
 * アプリケーション中で「,」「\\t」「\\n」を含むパスを扱う場合には、
 * 本関数で事前にセパレータに変更しておく必要があります。<br>
 * <br>
 * フィルターに指定できる文字の最大数は、現状 7 文字までです。<br>
 */
CriError CRIAPI criFsBinder_SetPathSeparatorForBindFiles(const CriChar8 *filter);

/*==========================================================================
 *      CriFsLoader API
 *=========================================================================*/
/*JP
 * \brief CRI File System - Loader オブジェクト
 * \ingroup FSLIB_BASIC
 * \par 説明:
 * CriFsLoaderとはファイルデータを簡単に読み出すためのモジュールです。
 */
/*EN
 * \brief CRI File System - Loader Object
 * \ingroup FSLIB_BASIC
 * \par
 * CriFsLoader is loading module. It is very simple and easy.
 */

/**
 * \brief CriFsLoaderの作成
 * \ingroup FSLIB_BASIC
 * \param[out]	loader	CriFsLoaderハンドル
 * \return	CriError	エラーコード
 * \par 説明:
 * CriFsLoaderを作成します。
 */
/*EN
 * \brief Create a CriFsLoader
 * \ingroup FSLIB_BASIC
 * \param[out]	loader	CriFsLoader handle
 * \return	CriError	Error information
 * \par
 * This function creates a CriFsLoader.
 */
CriError CRIAPI criFsLoader_Create(CriFsLoaderHn *loader);

/**
 * \brief CriFsLoaderの破棄
 * \ingroup FSLIB_BASIC
 * \param[in]	loader	CriFsLoaderハンドル
 * \return	CriError	エラーコード
 * \par 説明:
 * CriFsLoaderを破棄します。
 * \attention
 * 本関数をロード完了コールバック内で実行しないでください。
 * \sa criFsLoader_Create
 */
/*EN
 * \brief Destroy CriFsLoader
 * \ingroup FSLIB_BASIC
 * \param[in]	loader	CriFsLoader handle
 * \return	CriError	Error information
 * \par
 * This function deletes the CriFsLoader.
 * \atteniton
 * Do not call this function in the load end callback function.
 * ( ::CriFsLoaderLoadEndCbFunc ) <br>
 * \sa criFsLoader_Create
 */
CriError CRIAPI criFsLoader_Destroy(CriFsLoaderHn loader);

/**
 * \brief ロード完了コールバックの登録
 * \ingroup FSLIB_BASIC
 * \param[in]	loader	CriFsLoaderハンドル
 * \param[in]	func	コールバック関数
 * \param[in]	obj		コールバック関数へ渡す引数
 * \return	CriError	エラーコード
 * \par 説明:
 * ロード完了時に実行されるコールバック関数を登録します。<br>
 * ロード完了コールバックは、ローダーのステータスが CRIFSLOADER_STATUS_LOADING から
 * 他のステータスに遷移した直後に呼び出されます。<br>
 * （ CRIFSLOADER_STATUS_COMPLETE 以外にも、 CRIFSLOADER_STATUS_STOP や
 * CRIFSLOADER_STATUS_ERROR に遷移する際にもコールバックは実行されます。）<br>
 * \par 備考:
 * 厳密には、ステータス遷移～コールバック実行までの間に他の処理が割り込みで動作する
 * 余地があるため、ステータス遷移とコールバック実行のタイミングがズレる可能性があります。<br>
 * \attention
 * ロード完了コールバックは、 ::criFsLoader_Load 関数が CRIERR_OK
 * を返す場合にのみ呼び出されます。<br>
 * ::criFsLoader_Load 関数に不正な引数を渡した場合等、
 * ロード処理自体が開始されない場合（ローダーのステータスが CRIFSLOADER_STATUS_LOADING
 * にならない場合）には、ロード完了コールバック自体が実行されません。<br>
 * <br>
 * ロード処理やロード完了コールバックの呼び出しは、 ::criFsLoader_Load
 * 関数を実行したスレッドとは別のスレッドで実行される可能性があります。<br>
 * ほとんどの場合、他スレッドでのファイル読み込みが完了する前に ::criFsLoader_Load 関数から処理は復帰しますが、
 * 何らしかの理由で ::criFsLoader_Load 関数を実行するスレッドの動作が遅れた（または別スレッドでの処理が瞬時に完了した）場合、
 * ::criFsLoader_Load 関数から復帰する前にロード完了コールバックが呼び出される形になります。<br>
 * <br>
 * ロード完了後に呼び出されるロード完了コールバック関数は、
 * 『ロード開始時点で登録済みのコールバック関数』です。<br>
 * そのため、ロード処理開始～ロード完了までの間にロード完了コールバックを登録し直したとしても、
 * 呼び出される関数は変更されません。<br>
 * （登録し直したロード完了コールバック関数が使用されるのは、次回ロード処理実行時となります。）<br>
 * <br>
 * ロード完了コールバック内で参照されるオブジェクトは、
 * ローダーのステータスが完了状態（または停止状態）になった場合でも、
 * ロード完了コールバックが実行されるまでは解放しないでください。<br>
 * （ステータス遷移後、ロード完了コールバックが実行されるまでは、
 * 登録済みのコールバックパラメータが参照される可能性があります。）<br>
 * <br>
 * コールバックを登録した状態でロード処理を行っているローダーを
 * ::criFsLoader_Destroy 関数で破棄した場合、 ::criFsLoader_Destroy
 * 関数内でロード完了コールバックが実行される可能性があります。<br>
 * <br>
 * ロード完了コールバックを実行している間、他のファイルのロードがブロックされます。<br>
 * そのため、ロード完了コールバック内で負荷の高い処理を行なわないよう、ご注意ください。<br>
 * <br>
 * ロード完了コールバック内でローダーのステータス遷移を待つ処理を行わないでください。<br>
 * ローダーのステータス更新は、ロード完了コールバックと同一のスレッド上で行われます。<br>
 * そのため、ロード完了コールバック内でステータス遷移を待つ処理を行うと、
 * デッドロックが発生し、処理が進まなくなります。<br>
 * <br>
 * コールバック関数はローダー内のメモリ領域に対して登録されます。<br>
 * そのため、ロード完了コールバック内でローダーを破棄することはできません。<br>
 * （ロード完了コールバック内で ::criFsLoader_Destroy 関数を実行すると、
 * エラーが発生します。）<br>
 * <br>
 * コールバック関数は1つしか登録できません。<br>
 * 登録操作を複数回行った場合、既に登録済みのコールバック関数が、
 * 後から登録したコールバック関数により上書きされてしまいます。<br>
 * <br>
 * funcにNULLを指定するとことで登録済み関数の登録解除が行えます。<br>
 * \sa criFsLoader_Destroy
 */
CriError CRIAPI criFsLoader_SetLoadEndCallback(
	CriFsLoaderHn loader, CriFsLoaderLoadEndCbFunc func, void *obj);

/**
 * \brief データのロード
 * \ingroup FSLIB_BASIC
 * \param[in]	loader	CriFsLoaderハンドル
 * \param[in]	binder	CriFsBinderハンドル
 * \param[in]	path	ファイルパス名
 * \param[in]	offset	ファイルの先頭からのオフセット位置
 * \param[in]	load_size	ロードサイズ
 * \param[in]	buffer	バッファーへのポインター
 * \param[in]	buffer_size	バッファーのサイズ
 * \return	CriError	エラーコード
 * \par 説明:
 * 指定されたバインダーとファイル名で、データの読み込みを開始します。<br>
 * ファイル内の offset バイト目から、load_size バイト分読み込みます。<br>
 * 本関数は即時復帰関数です。<br>ロードの完了状態を取得するには criFsLoader_GetStatus関数を使用してください。<br>
 * また、本関数は指定された CriFsBinder ハンドルを介して CPK ファイル内の圧縮コンテンツを読み込むこともできます。<br>
 * \attention
 * 圧縮コンテンツの読み込みに際して、0 より大きなオフセット位置を指定した場合はエラーになります。<br>
 * また、圧縮コンテンツコンテンツを先頭から一部分だけ読み込むということもできません。コンテンツ全体を読み込んでください。
 * \sa criFsLoader_GetStatus
 */
/*EN
 * \brief Loading File Data
 * \ingroup FSLIB_BASIC
 * \param[in]	loader	CriFsLoader handle
 * \param[in]	binder	CriFsBinder handle
 * \param[in]	path 	File path
 * \param[in]	offset	Seek position from top of file
 * \param[in]	load_size	Loading size
 * \param[in]	buffer	buffer
 * \param[in]	buffer_size Size of buffer
 * \return	CriError	Error information
 * \par
 * This function starts loading of data by appointed File path and the binder.<br>
 * This function is return immediately.
 * You use an criFsLoader_GetStatus function to examine loading completion.<br>
 * \sa criFsLoader_GetStatus
 */
CriError CRIAPI criFsLoader_Load(CriFsLoaderHn loader,
	CriFsBinderHn binder, const CriChar8 *path, CriSint64 offset,
	CriSint64 load_size, void *buffer, CriSint64 buffer_size);

/**
 * \brief データのロード (CPKファイル内のファイルIDを指定)
 * \ingroup FSLIB_BASIC
 * \param[in]	loader	CriFsLoaderハンドル
 * \param[in]	binder	CriFsBinderハンドル
 * \param[in]	id ファイルID
 * \param[in]	offset	ファイルの先頭からのオフセット位置
 * \param[in]	load_size	ロードサイズ
 * \param[in]	buffer	バッファーへのポインター
 * \param[in]	buffer_size	バッファーのサイズ
 * \return	CriError	エラーコード
 * \par 説明:
 * 指定されたバインダーとファイルIDで、データの読み込みを開始します。<br>
 * ファイル内の offset バイト目から、load_size バイト分読み込みます。<br>
 * 本関数は即時復帰関数です。<br>ロードの完了状態を取得するには criFsLoader_GetStatus関数を使用してください。<br>
 * \sa criFsLoader_GetStatus
 */
/*EN
 * \brief Loading File Data (Using File ID in CPK File)
 * \ingroup FSLIB_BASIC
 * \param[in]	loader	CriFsLoader handle
 * \param[in]	binder	CriFsBinder handle
 * \param[in]	id	File ID
 * \param[in]	offset	Seek position from top of file
 * \param[in]	load_size	Loading size
 * \param[in]	buffer	buffer
 * \param[in]	buffer_size Size of buffer
 * \return	CriError	Error information
 * \par
 * This function starts loading of data by appointed File ID and the binder.<br>
 * This function is return immediately.
 * You use an criFsLoader_GetStatus function to examine loading completion.<br>
 * \sa criFsLoader_GetStatus
 */
CriError CRIAPI criFsLoader_LoadById(CriFsLoaderHn loader,
	CriFsBinderHn binder, CriFsFileId id, CriSint64 offset,
	CriSint64 load_size, void *buffer, CriSint64 buffer_size);

/**
 * \brief 圧縮データを展開せずにメモリ上にロード
 * \ingroup FSLIB_BASIC
 * \param[in]	loader	CriFsLoaderハンドル
 * \param[in]	binder	CriFsBinderハンドル
 * \param[in]	path	ファイルパス名
 * \param[in]	offset	ファイルの先頭からのオフセット位置
 * \param[in]	load_size	ロードサイズ
 * \param[in]	buffer	バッファーへのポインター
 * \param[in]	buffer_size	バッファーのサイズ
 * \return	CriError	エラーコード
 * \par 説明:
 * 指定されたバインダーとファイル名で、データの読み込みを開始します。<br>
 * ::criFsLoader_Load 関数と異なり、データが圧縮されている場合でも、
 * データを展開せずにメモリ上にロードします。<br>
 * <br>
 * 本関数は即時復帰関数です。<br>
 * ロードの完了状態を取得するには criFsLoader_GetStatus関数を使用してください。<br>
 * \sa criFsLoader_GetStatus
 */
CriError CRIAPI criFsLoader_LoadWithoutDecompression(CriFsLoaderHn loader,
	CriFsBinderHn binder, const CriChar8 *path, CriSint64 offset,
	CriSint64 load_size, void *buffer, CriSint64 buffer_size);

/**
 * \brief 圧縮データを展開せずにメモリ上にロード（CPKファイル内のファイルIDを指定）
 * \ingroup FSLIB_BASIC
 * \param[in]	loader	CriFsLoaderハンドル
 * \param[in]	binder	CriFsBinderハンドル
 * \param[in]	id ファイルID
 * \param[in]	offset	ファイルの先頭からのオフセット位置
 * \param[in]	load_size	ロードサイズ
 * \param[in]	buffer	バッファーへのポインター
 * \param[in]	buffer_size	バッファーのサイズ
 * \return	CriError	エラーコード
 * \par 説明:
 * 指定されたバインダーとファイルIDで、データの読み込みを開始します。<br>
 * ::criFsLoader_Load 関数と異なり、データが圧縮されている場合でも、
 * データを展開せずにメモリ上にロードします。<br>
 * <br>
 * 本関数は即時復帰関数です。<br>
 * ロードの完了状態を取得するには criFsLoader_GetStatus関数を使用してください。<br>
 * \sa criFsLoader_GetStatus
 */
CriError CRIAPI criFsLoader_LoadWithoutDecompressionById(CriFsLoaderHn loader,
	CriFsBinderHn binder, CriFsFileId id, CriSint64 offset,
	CriSint64 load_size, void *buffer, CriSint64 buffer_size);

/**
 * \brief メモリ上に配置された圧縮データの展開
 * \ingroup FSLIB_BASIC
 * \param[in]	loader	CriFsLoaderハンドル
 * \param[in]	src	圧縮データアドレス
 * \param[in]	src_size	圧縮データサイズ
 * \param[in]	dst	展開先メモリアドレス
 * \param[in]	dst_size	展開先メモリ領域サイズ
 * \return	CriError	エラーコード
 * \par 説明:
 * メモリ上に配置された圧縮データを、別のメモリ領域に展開します。<br>
 * <br>
 * 本関数は即時復帰関数です。<br>
 * ロードの完了状態を取得するには criFsLoader_GetStatus関数を使用してください。<br>
 * \par 備考:
 * 入力データが圧縮されていない場合、
 * 本関数は入力データを出力アドレスへそのままコピーします。<br>
 * \attention
 * 本関数を使用するには、Ver.2.19.21以降のCRI File System
 * Toolsを使用してデータを作成する必要があります。<br>
 * （データを作成したツールが古い場合、データが展開されません。）<br>
 * <br>
 * 本関数は、CRI独自のソフトウェア圧縮コーデックにしか対応していません。<br>
 * ハードウェアデコーダーを使用する場合や、プラットフォーム固有のコーデックを使用している場合、
 * 本関数ではデータを展開できません。<br>
 * \sa criFsLoader_GetStatus
 */
CriError CRIAPI criFsLoader_DecompressData(CriFsLoaderHn loader,
	void *src, CriSint64 src_size, void *dst, CriSint64 dst_size);

/**
 * \brief ロードの停止
 * \ingroup FSLIB_BASIC
 * \param[in]	loader	CriFsLoaderハンドル
 * \return	CriError	エラーコード
 * \par 説明:
 * ロードを停止します。<br>
 * 本関数は即時復帰関数です。停止状態を取得するには ::criFsLoader_GetStatus 関数を使用してください。<br>
 * \attention
 * 本関数を実行しても、ローダーのステータスが CRIFSLOADER_STATUS_STOP に変わるまでは、バッファーへのデータ転送が続いている可能性があります。<br>
 * ステータスが更新されるまでは、データロード先のバッファーを解放しないでください。<br>
 * \sa criFsLoader_GetStatus
 */
/*EN
 * \brief Stop Loading
 * \ingroup FSLIB_BASIC
 * \param[in]	loader	CriFsLoader handle
 * \return	CriError	Error information
 * \par
 * This function stops the loading.
 * \sa criFsLoader_GetStatus
 */
CriError CRIAPI criFsLoader_Stop(CriFsLoaderHn loader);

/**
 * \brief ロードステータスの取得
 * \ingroup FSLIB_BASIC
 * \param[in]	loader	CriFsLoaderハンドル
 * \param[out]	status	ロードステータス
 * \return	CriError	エラーコード
 * \par 説明:
 * ロードステータスを取得します。
 * \image html fs_state_transition.png ファイルシステムローダーの状態遷移図
 */
/*EN
 * \brief Get Loading status
 * \ingroup FSLIB_BASIC
 * \param[in]	loader	CriFsLoader handle
 * \param[out]	status	Loading status
 * \return	CriError	Error information
 * \par
 * This function returns Loading status.
 * \image html fs_state_transition.png Transition of File System Loader Status
 */
CriError CRIAPI criFsLoader_GetStatus(CriFsLoaderHn loader, CriFsLoaderStatus *status);

/**
 * \brief I/Oエラーコードの取得
 * \ingroup FSLIB_BASIC
 * \param[in]	loader	CriFsLoaderハンドル
 * \param[out]	io_err	I/Oエラーコード
 * \return	CriError	エラーコード
 * \par 説明:
 * I/Oのエラーコードを取得します。<br>
 * ::criFsLoader_GetStatus 関数がステータスがエラー状態になった場合、
 * 本関数を実行することでI/Oインターフェイスから返されたエラーコードを
 * 取得することが可能です。
 * \sa criFsLoader_GetStatus
 */
/*EN
 * \brief Get I/O error code
 * \ingroup FSLIB_BASIC
 * \param[in]	loader	CriFsLoader handle
 * \param[out]	io_err	I/O error code
 * \return	CriError	Error information
 * \par
 * This function returns I/O error code.<br>
 * When the ::criFsLoader_GetStatus function returns the error status,
 * the detail of the error is obtainable via this function.
 * \sa criFsLoader_GetStatus
 */
CriError CRIAPI criFsLoader_GetIoError(CriFsLoaderHn loader, CriFsIoError *io_err);

/**
 * \brief ロードサイズの取得
 * \ingroup FSLIB_BASIC
 * \param[in]	loader	CriFsLoaderハンドル
 * \param[out]	size	ロードサイズ
 * \return	CriError	エラーコード
 * \par 説明:
 * ロードされたサイズを取得します。<br>
 */
/*EN
 * \brief Get Loading size
 * \ingroup FSLIB_BASIC
 * \param[in]	loader	CriFsLoader handle
 * \param[out]	size	Loading size
 * \return	CriError	Error information
 * \par
 * This function returns Loading size.
 */
CriError CRIAPI criFsLoader_GetLoadSize(CriFsLoaderHn loader, CriSint64 *size);

/**
 * \brief ロード進行状況の取得
 * \ingroup FSLIB_BASIC
 * \param[in]	loader          CriFsLoaderハンドル
 * \param[out]	progress        ロード進行状況（読み込み済みサイズ）
 * \param[out]	request_size	ロード要求サイズ
 * \return	CriError	エラーコード
 * \par 説明:
 * ロード進行状況を取得します。<br>
 * 本関数で取得できる値は、ロード進行状況の確認、ロード失敗時のリジューム処理などに利用できます。
 */
/*EN
 * \brief Get Loading size
 * \ingroup FSLIB_BASIC
 * \param[in]	loader	CriFsLoader handle
 * \param[out]	size	Loading size
 * \return	CriError	Error information
 * \par
 * This function gets progress of the loader.
 */
CriError CRIAPI criFsLoader_GetProgress(CriFsLoaderHn loader, CriSint64* progress, CriSint64* request_size);

/**
 * \brief プライオリティの取得
 * \ingroup FSLIB_BASIC
 * \param[in]	loader	CriFsLoaderハンドル
 * \param[out]	prio	読み込みプライオリティ
 * \return	CriError	エラーコード
 * \par 説明:
 * データロードのプライオリティを取得します。
 * \sa criFsLoader_SetPriority
 */
/*EN
 * \brief Get a Priority
 * \ingroup FSLIB_BASIC
 * \param[in]	loader	CriFsLoader handle
 * \param[out]	priority	Priority of loader
 * \return	CriError	Error information
 * \par
 * This function gets priority of the loader.
 */
CriError CRIAPI criFsLoader_GetPriority(CriFsLoaderHn loader, CriFsLoaderPriority *prio);

/**
 * \brief プライオリティの設定
 * \ingroup FSLIB_BASIC
 * \param[in]	loader	CriFsLoaderハンドル
 * \param[in]	prio	読み込みプライオリティ
 * \return	CriError	エラーコード
 * \par 説明:
 * データロードのプライオリティを設定します。<br>
 * 複数のローダーに対し、同時に ::criFsLoader_Load 関数でロードを実行した場合、
 * プライオリティの高いローダーが先に読み込みを行います。<br>
 * また、既に低プライオリティのローダーが巨大なデータを読み込んでいる最中でも、
 * 後から高プライオリティのローダーの読み込みを開始すれば、低プライオリティのローダーの処理に割り込んで、
 * 高プライオリティのローダーの読み込みが先に実行されます。<br>
 * \par 備考:
 * 複数のローダーが全て同一プライオリティであった場合、
 * データの読み込みは ::criFsLoader_Load 関数を実行した順に処理されます。<br>
 * \attention
 * ファイルの読み込みが行なわれていない状態でロードを開始した場合、
 * プライオリティに関係なく、そのロード処理が即座に開始されます。<br>
 * そのため、ファイルの読み込みが行なわれていない状態で低プライオリティローダーの読み込みを行なった場合、
 * 直後に高プライオリティのローダーで読み込みを開始したとしても、
 * 低プライオリティローダーの読み込みがある程度行なわれることになります。<br>
 * （単位読み込みサイズ分のデータを処理するまでは、他のローダーに処理がスイッチすることはありません。）<br>
 * \sa criFsLoader_GetPriority, criFsLoader_Load, criFsLoader_SetReadUnitSize
 */
/*EN
 * \brief Set a Priority
 * \ingroup FSLIB_BASIC
 * \param[in]	loader	CriFsLoader handle
 * \param[in]	priority	Priority of loader
 * \return	CriError	Error information
 * \par
 * This function sets priority of the loader.
 */
CriError CRIAPI criFsLoader_SetPriority(CriFsLoaderHn loader, CriFsLoaderPriority prio);

/**
 * \brief 単位読み込みサイズの設定
 * \ingroup FSLIB_BASIC
 * \param[in]	loader	CriFsLoaderハンドル
 * \param[in]	unit_size	単位読み込みサイズ
 * \return	CriError	エラーコード
 * \par 説明:
 * 単位読み込みサイズを設定します。
 * CriFsLoaderは、大きなサイズのリード要求を処理する際、それを複数の小さな単位のリード処理に分割して連続実行します。<br>
 * この関数を使用することで単位リード処理サイズを変更することが可能です。<br>
 * リード要求のキャンセルや、高プライオリティのリード処理の割り込み等は、単位リードサイズ境界でのみ処理されます。<br>
 * そのため、ユニットサイズを小さく設定すると、I/O処理のレスポンスが向上します。逆に、ユニットサイズを大きく設定すると、ファイル単位の読み込み速度が向上します。
 */
/*EN
 * \brief Set Read Unit Size
 * \ingroup FSLIB_BASIC
 * \param[in]	loader	CriFsLoader handle
 * \param[in]	unit_size	The size of read unit
 * \return	CriError	Error information
 * \par
 * When loading large data, CriFsLoader divides it into multiple small unit, and reads them.<br>
 * This function sets the size of read units.<br>
 * Interrupt request, such as cancel of current load process, can be applied only at unit boundary.<br>
 * Therefore, if the unit size is smaller, the response of I/O process becomes better. If the unit size is larger, the speed of single reading becomes better.
 */
CriError CRIAPI criFsLoader_SetReadUnitSize(CriFsLoaderHn loader, CriSint64 unit_size);

/**
 * \brief ロードリミッタ番号の設定
 * \ingroup FSLIB_BASIC_EMB
 * \param[in]	loader	        CriFsLoaderハンドル
 * \param[in]	limiter_no		ロードリミッタ番号
 * \return	CriError			エラーコード
 * \par 説明:
 * CriFsLoaderハンドルにロードリミッタ番号を割り当てます。<br>
 * 共通のリミッタ番号を割り当てた全てのローダー、グループローダー、バッチローダーの合計読み込みサイズが制限されます。<br>
 * \attention
 * ゲーム機向けではロードリミッタ機能は非サポートです。この関数は呼び出さないでください。<br>
 * \sa CriFsLoadLimiterNo criFs_SetLoadLimiterSize criFs_SetLoadLimiterUnit criFsLoader_SetLoadLimiter criFsGroupLoader_SetLoadLimiter criFsBatchLoader_SetLoadLimiter
 */
CriError CRIAPI criFsLoader_SetLoadLimiter(CriFsLoaderHn loader, CriFsLoadLimiterNo limiter_no);

/*==========================================================================
 *      CriFsGroupLoader API
 *=========================================================================*/
/*JP
 * \brief CRI File System - Group Loader オブジェクト
 * \ingroup FSLIB_GROUPLOADER
 * \par 説明:
 * CriFsGroupLoaderとは、CPKファイル内でグループとして関連付けられた一連のファイルを、
 * 一括して読み出すためのモジュールです。
 */

/**
 * \brief グループローダーの作成
 * \ingroup FSLIB_GROUPLOADER
 * \param[in]	binder_id		バインドID
 * \param[in]	groupname		グループ名
 * \param[in]	attrname		アトリビュート名
 * \param[out]	grouploaderhn	グループローダーハンドル
 * \return		CriError		エラーコード
 * \par 説明：
 * バインドIDからグループローダーを作成し、グループローダーハンドルを返します。<br>
 * 本関数は完了復帰関数です。<br><br>
 * グループ情報付きのCPKファイルをバインドしたバインドIDが必要です。<br>
 * 指定されたグループ名やアトリビュート名が存在しない場合、グループローダーを作成しません。<br>
 * グループローダーが扱うグループ名とアトリビュート名は、グループローダー作成後に変更することは
 * できません。<br>
 * 他のグループ＋アトリビュートを扱う場合、別のグループローダーを作成します。<br><br>
 * アトリビュート名にNULLを指定した場合、指定グループに属する全てのファイルがグループロードの対象となります。<br>
 * また、パッキングツールのアトリビュート指定を「none」とした場合も、アトリビュート名にNULLを指定します。<br>
 * \par 例：
 * \code
 * CriFsBindId bndrid;
 * CriFsGroupLoaderHn gldrhn;
 * // グループ情報付きCPKファイル"group.cpk"のバインド
 * criFsBinder_BindCpk(bndrhn, NULL, "group.cpk", wk, wksz, &bndrid);
 * 	:
 * // グループ"GROUP1", アトリビュート"IMG"を扱うグループローダーを作成。
 * criFsGroupLoader_Create(bndrid, "GROUP1", "IMG", &gldrhn);
 * \endcode
 * \code
 * // グループ"GROUP"の全てのファイルを扱うグループローダーを作成。
 * criFsGroupLoader_Create(bndrid, "GROUP", NULL, &gldrhn);
 * \endcode
 * \sa criFsGroupLoader_Destroy()
 */
CriError CRIAPI criFsGroupLoader_Create(CriFsBindId binder_id, const CriChar8 *groupname, const CriChar8 *attrname, CriFsGroupLoaderHn *grouploaderhn);

/**
 * \brief バインダーハンドル対応のグループローダーの作成
 * \ingroup FSLIB_GROUPLOADER
 * \param[in]     bndrhn        バインダーハンドル(複数CPKファイルをバインドしているバインダー)
 * \param[in]     groupname     グループ名(NULL指定は不可)
 * \param[in]     attrname      アトリビュート名(NULL指定可能)
 * \param[out]    grouploaderhn グループローダーハンドル
 * \return        CriError      エラーコード
 * \par 説明:
 * バインダーハンドルからグループローダーを作成し、グループローダーハンドルを返します。<br>
 * この関数を利用すると、グループロード機能とマルチバインド機能を併用することができます。<br>
 * フルビルドしたCPKファイルと、差分ビルドしたCPKファイルをマルチバインドすることで、一部のコンテンツファイルを更新した場合のCPKビルド時間やCPKファイル転送時間を節約することができます。<br>
 * １回のグループロードは、どれか１つのCPKファイルが読み込み対象となります。この制約によって、グループロードの最大のメリットである一括処理の動作が維持されています。<br>
 * よって、更新されたコンテンツファイルで差分CPKファイルを作成してマルチバインドする場合は、グループ単位で（変更のないコンテンツファイルも含めて）差分CPKファイルに含めておく必要があります。<br>
 * 差分CPKファイルの情報では、グループ内の全ファイルが削除されたこととグループに変更が無かったことが区別できません。削除やリネームで消えたはずのグループでも、フルビルドCPKからロード成功してしまいます。この動作が好ましくない場合、フルビルドCPKを作り直すことで解消できます。<br>
 * 本関数を利用する場合は、criFsBinder_SetPriority 関数で明示的に優先度設定を行うことを推奨します。フルビルドCPKと差分CPKをマルチバインドする場合、差分CPKのプライオリティを高く設定してください。 <br>
 */
CriError CRIAPI criFsGroupLoader_CreateFromBinderHn(
					CriFsBinderHn bndrhn, const CriChar8 *groupname, const CriChar8 *attrname, CriFsGroupLoaderHn *grouploaderhn);

/**
 * \brief グループローダーの破棄
 * \ingroup FSLIB_GROUPLOADER
 * \param[in]	grouploaderhn	グループローダーハンドル
 * \return		CriError		エラーコード
 * \par 説明：
 * グループローダーを破棄します。<br>
 * 本関数は完了復帰関数です。<br>
 * グループロード中に本関数を呼び出した場合はロードを中断し、ロードのためにグループローダー内部で確保していた
 *  ::CriFsLoaderHn を解放します。<br>
 * グループロード中のグループローダーを破棄する場合、内部の ::CriFsLoaderHn が停止するのを待ちますので、
 * 本関数から復帰するまでに時間がかかる場合があります。<br>
 * これを回避するには、グループローダーのステータスが ::CRIFSLOADER_STATUS_LOADING でないことを確認してから、
 * 本関数を呼出します。
 * \sa criFsGroupLoader_Create() criFsGroupLoader_GetStatus()
 */
CriError CRIAPI criFsGroupLoader_Destroy(CriFsGroupLoaderHn grouploaderhn);

/**
 * \brief ロード開始コールバック関数の設定
 * \ingroup FSLIB_GROUPLOADER
 * \param[in]	grouploaderhn	グループローダーハンドル
 * \param[in]	func			グループローダーコールバック関数
 * \param[in]	obj				グループローダーコールバック関数引数
 * \return	CriError			エラーコード
 * \par 説明：
 * グループロード実施時にファイル毎に呼び出されるコールバック関数を設定します。<br>
 * 本関数で設定したコールバック関数は、ロードリクエストを行う前にファイル毎に呼出されます
 * （つまり、ファイル数と同じ回数コールバック関数が呼出されます）。<br>
 * グループロードコールバック関数を設定した場合、ファイル毎にコールバック関数を呼出すため、
 * 複数ファイルの一括ロードは行えません。<br>
 * グループロードコールバック関数にNULLを設定した場合、コールバック関数の設定は解除されます。<br>
 * <br>
 * ●コールバック関数について<br>
 * コールバック関数は引数として、obj:ユーザーが指定したオブジェクトと、gfinfo:ロードするファイルの
 * 情報構造体が渡されます。<br>
 * コールバック関数の返値は、そのファイルを読み込むバッファーへのポインターとなります。<br>
 * ファイルの読込を行いたくない場合は、 NULL を返値としてください。<br>
 * \attention
 * コールバック関数は1つしか登録できません。<br>
 * 登録操作を複数回行った場合、既に登録済みのコールバック関数が、
 * 後から登録したコールバック関数により上書きされてしまいます。<br>
 * <br>
 * funcにNULLを指定するとことで登録済み関数の登録解除が行えます。<br>
 * \sa criFsGroupLoader_LoadBulk()
 */
CriError CRIAPI criFsGroupLoader_SetLoadStartCallback(
	CriFsGroupLoaderHn grouploaderhn, CriFsGroupLoaderLoadStartCbFunc func, void *obj);

/**
 * \brief グループファイル数の取得
 * \ingroup FSLIB_GROUPLOADER
 * \param[in]	grouploaderhn	グループローダーハンドル
 * \param[out]	nfiles			グループファイル数
 * \return		CriError		エラーコード
 * \par 説明：
 * 指定のグループに属するファイル数を取得します。<br>
 * ::criFsGroupLoader_LoadBulk 関数の引数 gfinf の配列の要素数は、本関数で取得される
 * グループファイル数となります。
 * \sa criFsGroupLoader_GetTotalGroupDataSize(), criFsGroupLoader_LoadBulk()
 */
CriError CRIAPI criFsGroupLoader_GetNumberOfGroupFiles(
	CriFsGroupLoaderHn grouploaderhn, CriSint32 *nfiles);

/**
 * \brief グループデータサイズの取得
 * \ingroup FSLIB_GROUPLOADER
 * \param[in]	grouploaderhn	グループローダーハンドル
 * \param[out]	datasize 		データサイズ
 * \return		CriError		エラーコード
 * \par 説明：
 * グループロードに必要な読込領域のサイズを取得します。<br>
 * アライメントなども加味されたデータサイズとなります。
 * \sa criFsGroupLoader_GetNumberOfGroupFiles(), criFsGroupLoader_LoadBulk()
 */
CriError CRIAPI criFsGroupLoader_GetTotalGroupDataSize(
	CriFsGroupLoaderHn grouploaderhn, CriSint64 *datasize);

/**
 * \brief グループファイル情報の取得
 * \ingroup FSLIB_GROUPLOADER
 * \param[in]	grouploaderhn	グループローダーハンドル
 * \param[out]	gfinf[]			CriFsGroupFileInfo構造体の配列
 * \param[in]	numgfinf			配列(gfinf[])の要素数
 * \return		CriError		エラーコード
 * \par 説明：
 * グループ化された複数ファイルデータ情報を取得します。<br>
 * ファイル数分の ::CriFsGroupFileInfo 構造体の配列を指定する必要があります。<br>
 * パッキングツールのアトリビュート指定を「none」としたグループに対しては、本関数に指定するグループ
 * ローダー作成時のアトリビュート名指定をNULLとして下さい。<br>
 * 取得されるグループファイル情報( ::CriFsGroupFileInfo )の内、datapointerで指される読込先はNULLとなります。
 * \par 例：
 * \code
 * CriSint32 nfiles;
 * CriFsGroupFileInfo *gfinf;
 * // グループファイル情報構造体配列領域の確保
 * criFsGroupLoader_GetNumberOfGroupFiles(gldrhn, &nfiles);
 * gfinf = malloc( sizeof(CriFsGroupFileInfo) * nfiles );
 * // グループファイル読み込み領域の確保
 * criFsGroupLoader_GetTotalGroupDataSize(gldrhn, &datasize);
 * databuff = malloc(datasize);
 * // グループロード情報の取得
 * criFsGroupLoader_GetGroupFileInfos(gldrhn, gfinf, nfiles);
 * \endcode
*/
CriError CRIAPI criFsGroupLoader_GetGroupFileInfos(
	CriFsGroupLoaderHn grouploaderhn, CriFsGroupFileInfo gfinf[], CriSint32 numgfinf);

/**
 * \brief グループロードの開始
 * \ingroup FSLIB_GROUPLOADER
 * \param[in]	gourploaderhn	グループローダーハンドル
 * \param[out]	buffer			ロード先バッファーへのポインター
 * \param[in]	buffer_size		ロード先バッファーのサイズ
 * \param[out]	gfinf[]			CriFsGroupInfo構造体の配列
 * \param[in]	numgfinf			配列(gfinf[])の要素数
 * \return		CriError		エラーコード
 * \par 説明：
 * グループ化された複数ファイルデータの読み込みを開始します。<br>
 * ファイル数分の ::CriFsGroupFileInfo 構造体の配列を指定する必要があります。<br>
 * 指定のグループファイルを読み込めるサイズのロード領域が必要です。<br>
 * 本関数は即時復帰関数です。<br>ロードの完了状態を取得するには ::criFsGroupLoader_GetStatus 関数を使用してください。<br><br>
 * パッキングツールのアトリビュート指定を「none」としたグループを読み込む場合、本関数に指定するグループ
 * ローダーは、グループローダー作成時にアトリビュート名指定をNULLとします。<br><br>
 * グループロードコールバック関数を設定した場合、コールバック関数の返値をロードアドレスとしますので、
 * 本関数の引数 buffer, buffer_size は参照されません。<br>
 * なお指定した gfinf はロード後も ::criFsGroupLoader_GetGroupFileInfoIndex 関数等の情報取得関数にてライブラリから参照を行います。<br>
 * そのため基本的にはハンドル破棄まで領域は保持しておく必要があります。<br><br>
 * グループローダー内部で複数の ::CriFsLoaderHn を利用してロードを行います。 ::CriFsLoaderHn を１つも作成
 * できない場合、エラーコールバックが呼ばれます。グループローダーで作成した ::CriFsLoaderHn は、
 * グループロード完了時に破棄されます。
 * \par 例：
 * \code
 * CriSint32 nfiles;
 * CriFsGroupFileInfo *gfinf;
 * CriSint64 datasize;
 * void *databuff;
 * // グループファイル情報構造体配列領域の確保
 * criFsGroupLoader_GetNumberOfGroupFiles(gldrhn, &nfiles);
 * gfinf = malloc( sizeof(CriFsGroupFileInfo) * nfiles );
 * // グループファイル読み込み領域の確保
 * criFsGroupLoader_GetTotalGroupDataSize(gldrhn, &datasize);
 * databuff = malloc(datasize);
 * // グループロード
 * criFsGroupLoader_LoadBulk(gldrhn, databuff, datasize, gfinf, nfiles);
 * // グループロード完了待ち
 * for (;;) {
 * 	CriFsLoaderStatus status;
 * 	criFsGroupLoader_GetStatus(gldrhn, &status);
 * 	if (status == CRIFSLOADER_STATUS_COMPLETE) break;
 * }
 * \endcode
 * \sa criFsGroupLoader_GetStatus() criFsGroupLoader_SetLoadStartCallback()  criFsGroupLoader_Stop() criFsGroupLoader_Create()
 */
CriError CRIAPI criFsGroupLoader_LoadBulk(CriFsGroupLoaderHn gourploaderhn, void *buffer, CriSint64 buffer_size, CriFsGroupFileInfo gfinf[], CriSint32 numgfinf);

/**
 * \brief グループロードの停止（中断）
 * \ingroup FSLIB_GROUPLOADER
 * \param[in]	grouploaderhn	グループローダーハンドル
 * \return		CriError		エラーコード
 * \par 説明：
 * グループローダーでのファイルロードを中断します。<br>
 * グループロードを中断するまでにロードされたファイル数やファイルの内容はそのまま保持されます。<br>
 * <br>
 * 本関数は即時復帰関数です。<br>
 * グループロードの際、グループローダー内部で ::CriFsLoaderHn を作成してファイルロードを行います。
 * 本関数を呼出した場合、グループロードに使用している ::CriFsLoaderHn に中断（Stop）の指示を出して
 * 復帰します。<br>
 * そのため、本関数復帰時点では、まだファイルロード中である可能性があります。<br>
 * グループローダーは、内部で使用している ::CriFsLoaderHn のロード停止確認後に、その ::CriFsLoaderHn を
 * 解放し、グループローダーのステータスを ::CRIFSLOADER_STATUS_STOP にします。<br>
 * この一連の処理は ::criFsGroupLoader_GetStatus 関数呼び出し時に行われますので、本関数呼び出し後は、
 * グループローダーのステータスが ::CRIFSLOADER_STATUS_LOADING でないことを確認してください。<br>
 * そうしない場合、グループローダー内部で使用している ::CriFsLoaderHn が解放されませんので、
 * 他のグループローダーが ::CriFsLoaderHn を確保できなくなる可能性があります。
 * <br>
 * \sa criFsGroupLoader_GetStatus() criFsGroupLoader_LoadBulk()
 */
CriError CRIAPI criFsGroupLoader_Stop(CriFsGroupLoaderHn grouploaderhn);

/**
 * \brief ロードステータスの取得
 * \ingroup FSLIB_GROUPLOADER
 * \param[in]	grouploaderhn	グループローダーハンドル
 * \param[out]	status			CriFsGroupLoaderStatusロードステータス
 * \return		CriError		エラーコード
 * \par 説明：
 * グループローダーのロードステータスを返します。<br>
 * グループロード対象の全てのファイルのロードを完了した場合、
 * ::CRIFSLOADER_STATUS_COMPLETE を返します。
 * \sa criFsGroupLoader_LoadBulk()
 */
CriError CRIAPI criFsGroupLoader_GetStatus(CriFsGroupLoaderHn grouploaderhn, CriFsLoaderStatus *status);

/**
 * \brief ロードされたファイル数の取得
 * \ingroup FSLIB_GROUPLOADER
 * \param[in]	grouploaderhn	グループローダーハンドル
 * \param[out]	nfiles			ロードされたファイル数
 * \return		CriError		エラーコード
 * \par 説明：
 * ::criFsGroupLoader_LoadBulk 関数で既に読み込まれたファイルの数を返します。
 * \sa criFsGroupLoader_LoadBulk()
 */
CriError CRIAPI criFsGroupLoader_GetLoadedFiles(CriFsGroupLoaderHn grouploaderhn, CriSint32 *nfiles);

/**
 * \brief ファイルの読込状態の取得
 * \ingroup FSLIB_GROUPLOADER
 * \param[in]	gfinfo		読込状態を取得するファイルの CriFsGroupFileInfo 構造体へのポインター
 * \param[out]	result		ファイルの読込状態（CRI_TRUE：読込済 CRI_FALSE:未読込)
 * \return		CriError	エラーコード
 * \par 説明：
 * 指定されたファイルが読み込まれたかどうかを取得します。<br>
 * 読込状態を取得するファイルの ::CriFsGroupFileInfo 構造体へのポインターは
 * ::criFsGroupLoader_GetGroupFileInfoIndex 関数や
 * ::criFsGroupLoader_GetGroupFileInfo 関数で取得します。
 * \sa criFsGroupLoader_GetGroupFileInfoIndex(), criFsGroupLoader_GetGroupFileInfo()
 */
CriError CRIAPI criFsGroupLoader_IsLoaded(const CriFsGroupFileInfo *gfinfo, CriBool *result);

/**
 * \brief CriFsGroupFileInfo構造体の配列インデクスの取得
 * \ingroup FSLIB_GROUPLOADER
 * \param[in]	grouploaderhn	グループローダーハンドル
 * \param[in]	fpath			ファイル名フルパス
 * \param[out]	index 			配列インデクス
 * \return		CriError		エラーコード
 * \par 説明：
 * 指定されたファイルの ::CriFsGroupFileInfo 構造体の配列インデクスを取得します。<br>
 * ファイル名はフルパス名で指定します。指定されたファイルの検索にはバイナリサーチを使用します。<br>
 * 指定されたファイルが見つからない場合は、返値が-1となります。<br>
 * グループロードされた個々のファイルが実際にロードされたアドレスは、
 * ::CriFsGroupFileInfo 構造体に記述されます。<br>
 * ロードされたデータへアクセスするには、::CriFsGroupFileInfo 構造体を取得する必要があります。<br>
 * ::CriFsGroupFileInfo 構造体を取得する方法には、ロードしたファイルのファイル名もしくは
 * コンテンツファイルIDを指定し、該当する構造体要素を取得する方法と、本関数で取得したインデクスにより
 * 構造体配列を直接アクセスする方法の２通りがあります。
 * \sa criFsGroupLoader_GetNumberOfGroupFiles(), criFsGroupLoader_GetGroupFileInfo()
 */
CriError CRIAPI criFsGroupLoader_GetGroupFileInfoIndex(CriFsGroupLoaderHn grouploaderhn,  const CriChar8 *fpath, CriSint32 *index);

/**
 * \brief CriFsGroupFileInfo構造体の配列インデクスの取得(ID指定)
 * \ingroup FSLIB_GROUPLOADER
 * \param[in]	grouploaderhn	グループローダーハンドル
 * \param[in]	id				コンテンツファイルID
 * \param[out]	index 			配列インデクス
 * \return		CriError		エラーコード
 * \par 説明：
 * 指定されたファイルの ::CriFsGroupFileInfo 構造体の配列インデクスを取得します。<br>
 * ファイルはコンテンツファイルIDで指定します。指定されたファイルの検索にはリニアサーチを使用しています。<br>
 * 指定されたファイルが見つからない場合は、返値が-1となります。<br>
 * グループロードされた個々のファイルが実際にロードされたアドレスは、
 * ::CriFsGroupFileInfo 構造体に記述されます。<br>
 * 検索方法の都合上、既にグループロード済で ::CriFsGroupFileInfo 情報を取得している場合は、
 *  ::CriFsGroupFileInfo 情報のIDを直接検索する方が、本関数でインデクスを取得するよりも検索効率が高いです。<br>
 * ロードされたデータへアクセスするには、::CriFsGroupFileInfo 構造体を取得する必要があります。<br>
 * ::CriFsGroupFileInfo 構造体を取得する方法には、ロードしたファイルのファイル名もしくは
 * コンテンツファイルIDを指定し、該当する構造体要素を取得する方法と、本関数で取得したインデクスにより
 * 構造体配列を直接アクセスする方法の２通りがあります。
 * \sa criFsGroupLoader_GetNumberOfGroupFiles(), criFsGroupLoader_GetGroupFileInfoById()
 */
CriError CRIAPI criFsGroupLoader_GetGroupFileInfoIndexById(CriFsGroupLoaderHn grouploaderhn, CriFsFileId id, CriSint32 *index);

/**
 * \brief CriFsGroupFileInfo構造体の取得
 * \ingroup FSLIB_GROUPLOADER
 * \param[in]	grouploaderhn	グループローダーハンドル
 * \param[in]	fpath 			ファイル名フルパス
 * \param[out]	gfinfo			CriFsGroupFileInfo構造体へのポインターへのポインター
 * \return		CriError 		エラーコード
 * \par 説明：
 * 指定されたファイルの ::CriFsGroupFileInfo 構造体へのポインターを取得します。<br>
 * ファイル名はフルパス名で指定します。<br>
 * 指定されたファイルが見つからない場合は、出力値が NULL となります。
 * \sa criFsGroupLoader_GetNumberOfGroupFiles(), criFsGroupLoader_GetGroupFileInfoIndex()
 */
CriError CRIAPI criFsGroupLoader_GetGroupFileInfo(CriFsGroupLoaderHn grouploaderhn, const CriChar8 *fpath, const CriFsGroupFileInfo **gfinfo);

/**
 * \brief CriFsGroupFileInfo構造体の取得(ID指定)
 * \ingroup FSLIB_GROUPLOADER
 * \param[in]	grouploaderhn	グループローダーハンドル
 * \param[in]	id	 			コンテンツファイルID
 * \param[out]	gfinfo			CriFsGroupFileInfo構造体へのポインターへのポインター
 * \return		CriError 		エラーコード
 * \par 説明：
 * 指定されたファイルの ::CriFsGroupFileInfo 構造体へのポインターを取得します。<br>
 * ファイルはコンテンツファイルIDで指定します。<br>
 * 指定されたファイルが見つからない場合は、出力値が NULL となります。
 * \sa criFsGroupLoader_GetNumberOfGroupFiles(), criFsGroupLoader_GetGroupFileInfoIndexById()
 */
CriError CRIAPI criFsGroupLoader_GetGroupFileInfoById(CriFsGroupLoaderHn grouploaderhn, CriFsFileId id, const CriFsGroupFileInfo **gfinfo);

/**
 * \brief グループ名の取得
 * \ingroup FSLIB_GROUPLOADER
 * \param[in]	grouploaderhn	グループローダーハンドル
 * \param[out]	groupname 		グループ名
 * \return		CriError 		エラーコード
 * \par 説明：
 * グループローダーで扱うグループのグループ名を取得します。
 */
CriError CRIAPI criFsGroupLoader_GetGroupName(CriFsGroupLoaderHn grouploaderhn, const CriChar8 **groupname);

/**
 * \brief グループ属性の取得
 * \ingroup FSLIB_GROUPLOADER
 * \param[in] 	grouploaderhn	グループローダーハンドル
 * \param[out]	attrname 		グループ属性
 * \return		CriError 		エラーコード
 * \par 説明：
 * グループローダーで扱うグループのグループ属性を取得します。
 */
CriError CRIAPI criFsGroupLoader_GetAttributeName(CriFsGroupLoaderHn grouploaderhn, const CriChar8 **attrname);

/**
 * \brief プライオリティの取得
 * \ingroup FSLIB_GROUPLOADER
 * \param[in]	grouploaderhn	CriFsGroupLoaderハンドル
 * \param[out]	prio	読み込みプライオリティ
 * \return	CriError	エラーコード
 * \par 説明:
 * グループローダー内で使用するローダー( ::CriFsLoaderHn )のプライオリティを取得します（初期値は ::CRIFSLOADER_PRIORITY_NORMAL です）。
 * ローダーのプライオリティに関しては、::criFsLoader_GetPriority や ::criFsLoader_SetPriority の説明も参照ください。
 * \sa criFsGroupLoader_GetLoaderPriority, criFsLoader_GetPriority, criFsLoader_SetPriority
 */
CriError CRIAPI criFsGroupLoader_GetLoaderPriority(CriFsGroupLoaderHn grouploaderhn, CriFsLoaderPriority *prio);

/**
 * \brief プライオリティの設定
 * \ingroup FSLIB_GROUPLOADER
 * \param[in]	grouploaderhn	CriFsGroupLoaderハンドル
 * \param[in]	prio	読み込みプライオリティ
 * \return	CriError	エラーコード
 * \par 説明:
 * グループローダー内で使用するローダー( ::CriFsLoaderHn )のプライオリティを設定します（初期値は ::CRIFSLOADER_PRIORITY_NORMAL です）。
 * ローダーのプライオリティに関しては、::criFsLoader_GetPriority や ::criFsLoader_SetPriority の説明も参照ください。
 * \sa criFsGroupLoader_SetLoaderPriority, criFsLoader_SetPriority
 */
CriError CRIAPI criFsGroupLoader_SetLoaderPriority(CriFsGroupLoaderHn grouploaderhn, CriFsLoaderPriority prio);

/**
 * \brief 単位読み込みサイズの設定
 * \ingroup FSLIB_GROUPLOADER
 * \param[in]	grouploaderhn	CriFsGroupLoaderハンドル
 * \param[in]	unit_size	単位読み込みサイズ
 * \return	CriError	エラーコード
 * \par 説明:
 * グループローダー内で使用するローダーの単位読み込みサイズを設定します。
 * CriFsLoaderは、大きなサイズのリード要求を処理する際、それを複数の小さな単位のリード処理に分割して連続実行します。<br>
 * この関数を使用することで単位リード処理サイズを変更することが可能です。<br>
 * リード要求のキャンセルや、高プライオリティのリード処理の割り込み等は、単位リードサイズ境界でのみ処理されます。<br>
 * そのため、ユニットサイズを小さく設定すると、I/O処理のレスポンスが向上します。逆に、ユニットサイズを大きく設定すると、ファイル単位の読み込み速度が向上します。
 */
CriError CRIAPI criFsGroupLoader_SetReadUnitSize(CriFsGroupLoaderHn grouploaderhn, CriSint64 unit_size);

/**
 * \brief ロードリミッタ番号の設定
 * \ingroup FSLIB_GROUPLOADER_EMB
 * \param[in]	grouploaderhn		CriFsGroupLoaderハンドル
 * \param[in]	limiter_no		ロードリミッタ番号
 * \return	CriError			エラーコード
 * \par 説明:
 * CriFsGroupLoaderハンドルにロードリミッタ番号を割り当てます。<br>
 * 共通のリミッタ番号を割り当てた全てのローダー、グループローダー、バッチローダーの合計読み込みサイズが制限されます。<br>
 * \attention
 * ゲーム機向けではロードリミッタ機能は非サポートです。この関数は呼び出さないでください。<br>
 * \sa CriFsLoadLimiterNo criFs_SetLoadLimiterSize criFs_SetLoadLimiterUnit criFsLoader_SetLoadLimiter criFsGroupLoader_SetLoadLimiter criFsBatchLoader_SetLoadLimiter
 */
CriError CRIAPI criFsGroupLoader_SetLoadLimiter(CriFsGroupLoaderHn grouploaderhn, CriFsLoadLimiterNo limiter_no);

/**
 * \brief サーバー処理当たりの準備ファイル数の制限
 * \ingroup FSLIB_GROUPLOADER
 * \param[in] grouploaderhn     グループローダーハンドル
 * \param[in] nfile_per_server  １サーバー当たりの準備処理ファイル数、または、特別値
 *                              CRIFS_GROUPLOADER_NO_PREPARATION_LIMIT
 * \return  CriError エラーコード
 * \par 説明：
 * グループローダーの準備処理の処理方法を設定します。<br>
 * この関数を呼び出さない場合、または、CRIFS_GROUPLOADER_NO_PREPARATION_LIMIT を指定した場合は、
 * criFsGroupLoader_LoadBulk 内部で準備処理を完結させます。この処理方法では、グループ内ファイル数が多い
 * 場合に criFsGroupLoader_LoadBulk 関数の処理時間が長くなり、コマ落ちが発生する場合があります。<br>
 * この問題を回避する手段として、グループローダーの準備処理をサーバー処理に分散させる機能を追加しました。<br>
 * 特別値以外の数値を指定した場合、準備処理は criFsGroupLoader_LoadBulk 関数からサーバー処理に移管され、
 * 1回のサーバー処理で準備処理を行うファイルの数は指定した数に制限されます。<br>
 * グループローダーのサーバー処理は、criFsGroupLoader_GetStatus 関数内で実行されます。<br>
 * <br>
 * 具体的なコードの例は以下のとおりです。
 * \par 例:
 * \code
 * {
 *   // ハンドルaの準備処理はLoadBulk関数内で行う。
 *   criFsGroupLoader_LimitNumPreparingFiles(grouploaderhn_a, CRIFS_GROUPLOADER_NO_PREPARATION_LIMIT);
 *   // ハンドルbとcの準備処理はサーバー処理で500ファイルづつ行う。ハンドル毎に個別に処理量を制限する。
 *   criFsGroupLoader_LimitNumPreparingFiles(grouploaderhn_b, 500);
 *   criFsGroupLoader_LimitNumPreparingFiles(grouploaderhn_c, 500);
 * }
 * \endcode
 */
CriError CRIAPI criFsGroupLoader_LimitNumPreparingFiles(CriFsGroupLoaderHn grouploaderhn, CriSint32 nfile_per_server);

/**
 * \brief 特定アトリビュートのCriFsGroupFileInfo構造体配列の開始オフセットの取得
 * \ingroup FSLIB_GROUPLOADER_EMB
 * \param[in] group_all   グループローダーハンドル(グループ全体)
 * \param[in] group_attr  グループローダーハンドル(特定アトリビュート)
 * \param[out] offset     グループ全体のCriFsGroupFileInfo構造体の配列に対する、特定アトリビュートが開始するオフセット
 * \return  CriError   エラーコード
 * \par 説明：
 * 下記の条件でグループローダーを利用する場合に、この関数を利用すると検索処理効率を改善することができます。<br>
 * \par 適用条件:
 * 　・グループ内に複数のアトリビュートのファイルを含める。<br>
 * 　・グループ内の複数のアトリビュートのファイルを同時にロードする。<br>
 * 　・アトリビュート毎に転送先を切り分ける。<br>
 * 　・アトリビュートを特定する情報は持たずに、ファイル名またはファイルIDからグループファイル情報を検索する。<br>
 * 　・グループの階層構造（サブグループ）は利用していない。<br>
 * 　・CPKファイル内に本APIが利用する追加情報が記録されている。（本APIのサポートは環境依存です）<br>
 * <br>
 * この関数を利用しない場合は、個々のファイルがどのアトリビュートに属するか分からないので、全てのグループハンドルを逐次検索しなければなりません。<br>
 * この関数を利用する場合は、１回の検索で目的のファイルを見つけることができます。<br>
 * グループロードを実行するハンドルは、グループ名とアトリビュート名を指定して、グループローダーハンドルを作成してください。<br>
 * さらに１つ、検索だけを目的とした、グループ全体のグループローダーハンドルを作成します。この時はグループ名だけを指定し、アトリビュート名にはCRI_NULLを指定してください。<br>
 * CriFsGroupFileInfo構造体配列については、グループ全体の配列を連続メモリとして確保してください。<br>
 * このグループ全体の配列を切り分けて、個々の criFsGroupLoader_LoadBulk 関数の引数に指定します。<br>
 * 個々のグループローダーの動作と、グループ全体に対する検索処理が整合するためには、グループ全体の配列を適切に切り分ける必要があります。<br>
 * 本関数が出力する「特定アトリビュートの開始オフセット」を利用して、全体の配列を切り分けてください。<br>
 * 具体的なコードの例を以下に示します。<br>
 * \par 例:
 * \code
 * {
 *   criFsGroupLoader_Create(bindid, "GROUP1", CRI_NULL, &group_all);
 *   criFsGroupLoader_Create(bindid, "GROUP1", "RAM1", &group_ram1);
 *   criFsGroupLoader_Create(bindid, "GROUP1", "RAM2", &group_ram2);
 *   criFsGroupLoader_GetNumberOfGroupFiles(group_all, &nfiles_all);
 *   criFsGroupLoader_GetNumberOfGroupFiles(group_ram1, &nfiles_ram1);
 *   criFsGroupLoader_GetNumberOfGroupFiles(group_ram2, &nfiles_ram2);
 *   criFsGroupLoader_GetGroupFileInfoStartOffset(group_all, group_ram1, &offset_ram1);
 *   criFsGroupLoader_GetGroupFileInfoStartOffset(group_all, group_ram2, &offset_ram2);
 *   gfinf_all = malloc(sizeof(CriFsGroupFileInfo) * nfiles_all);
 *   //memset(gfinf_all, 0, sizeof(CriFsGroupFileInfo) * nfiles_all);※ゼロクリアが必要なケースについて下記参照
 *   criFsGroupLoader_LoadBulk(group_ram1, buffer_ram1, buffer_size_ram1, &gfinf_all[offset_ram1], nfiles_ram1);
 *   criFsGroupLoader_LoadBulk(group_ram2, buffer_ram2, buffer_size_ram2, &gfinf_all[offset_ram2], nfiles_ram2);
 *   for (;;) {
 *     criFsGroupLoader_GetStatus(group_ram1, &status_ram1);
 *     criFsGroupLoader_GetStatus(group_ram2, &status_ram2);
 *     if (status_ram1 == CRIFSLOADER_STATUS_COMPLETE && 
 *         status_ram2 == CRIFSLOADER_STATUS_COMPLETE) {
 *       break;
 *     }
 *   }
 *   criFsGroupLoader_GetGroupFileInfoIndex(group_all, "sample.bmp", &index);
 *   userDrawBitmap(gfinf_all[index].datapointer);
 * }
 * \endcode
 * \attention
 * ※:グループに含まれる全てのアトリビュートをロード処理対象としない場合には注意が必要です。
 * ロードしなかったファイルでも検索が成功しますが、取得したインデックスで参照できる
 * CriFsGroupFileInfo構造体の内容は不定値となっています。
 * ロードしなかったファイルを検索する可能性があるなら、あらかじめ配列をゼロクリアしておき、
 * 構造体の内容が無効であることを識別できるようにしてください。<br>
 * \attention
 * 本APIのサポートは環境依存です。未サポートの環境で呼び出した場合はエラーとなります。<br>
 */
CriError CRIAPI criFsGroupLoader_GetGroupFileInfoStartOffset(CriFsGroupLoaderHn group_all, CriFsGroupLoaderHn group_attr, CriSint32 *offset);

/**
 * \brief 単一ファイルのロード確認の設定
 * \ingroup FSLIB_GROUPLOADER_EMB
 * \param[in]	use_flag		CRI_TRUE または CRI_FALSE
 * \par 説明:
 * criFsGroupLoader_IsLoaded関数を使用するかを設定します。<br>
 * CRI_TRUEを指定すると使用し、CRI_FALSEを指定すると使用しません<br>
 * criFsGroupLoader_IsLoaded関数を使用しない場合、ロード完了時の処理が早くなりまが、
 * 単一ファイルのロード確認が行えません。
 * criFsGroupLoader_LoadBulk 関数を実行する前に本関数を呼んでください。
 * \sa criFsGroupLoader_IsLoaded criFsGroupLoader_LoadBulk 
 */
void CRIAPI criFsGroupLoader_SetUseLoadedFlag(CriBool use_flag);

/*==========================================================================
 *      Log Output API
 *=========================================================================*/
/**
 * \brief ログ出力機能の追加
 * \ingroup FSLIB_CRIFS
 * \param[in] mode				ログ出力モード
 * \param[in] param				拡張パラメータ
 * \return	CriError 			エラーコード
 * \par 説明:
 * ログ出力機能を有効にし、ファイルアクセスログの出力を開始します。<br>
 * 本関数を実行すると、ファイルにアクセスするタイミングで、デバッガー等にファイルアクセスログが出力されるようになります。<br>
 * \attention
 * 本関数を実行後、必ず対になる ::criFs_DetachLogOutput 関数を実行してください。<br>
 * また、 ::criFs_DetachLogOutput 関数を実行するまでは、本関数を再度実行することはできません。<br>
 * \sa criFs_DetachLogOutput
 */
CriError CRIAPI criFs_AttachLogOutput(CriFsLogOutputMode mode, void *param);

/**
 * \brief ログ出力機能の削除
 * \ingroup FSLIB_CRIFS
 * \return	CriError 			エラーコード
 * \par 説明:
 * ログ出力機能を無効にし、ファイルアクセスログの出力を停止します。<br>
 * 本関数を実行することで、デバッガー等へのファイルアクセスログの出力を停止することが可能です。<br>
 * \attention
 * ::criFs_DetachLogOutput 関数実行前に本関数を実行することはできません。<br>
 * \sa criFs_AttachLogOutput
 */
CriError CRIAPI criFs_DetachLogOutput(void);

/**
 * \brief ユーザー定義ログ出力関数の登録
 * \ingroup FSLIB_CRIFS
 * \param[in]	func			ログ出力関数
 * \param[in]	obj				ログ出力関数に渡すオブジェクト
 * \return	CriError 			エラーコード
 * \par 説明:
 * ログの出力関数をユーザー指定の関数に置き換えます。<br>
 * 本関数を使用することで、ファイルアクセスログの出力方法をユーザーが自由に
 * カスタマイズすることが可能です。
 * \par 備考:
 * 本関数を使用していない場合や、ログ出力関数（func）にNULLを指定した場合、
 * CRI File Systemライブラリのデフォルトログ出力関数が使用されます。
 */
CriError CRIAPI criFs_SetUserLogOutputFunction(CriFsLogOutputFunc func, void *obj);

/*JP
 * \brief ロード区間の開始
 * \ingroup FSLIB_CRIFS
 * \param[in] name				ロード区間名
 * \return	CriError 			エラーコード
 * \par 説明:
 * ロード区間の開始を宣言します。<br>
 * ::criFs_AttachLogOutput 関数でファイルアクセスログの出力を有効にしている場合、本関数の引数（name）で指定したロード区間名がログに出力されます。<br>
 * ロード区間は、ファイルを最適に配置するための目安として使用されます。<br>
 * CPK File Builderでファイルアクセスログからグループを作成する場合、本関数で定義したロード区間がグループに変換されます。<br>
 * （同一ロード区間内でロードするファイル同士は、最適配置時に近い場所に配置される可能性が高くなります。）<br>
 * \attention
 * 複数のロード区間を重複させることはできません。<br>
 * 本関数を実行後、必ず対になる ::criFs_EndLoadRegion 関数を実行してください。
 * 本関数と ::criFs_BeginGroup 関数を併用することはできません。<br>
 * （CRI File System Ver.2.02.00より、本関数の機能が ::criFs_BeginGroup 関数に統合され、関数自体も ::criFs_BeginGroup 関数を呼び出すマクロに変更されました。）<br>
 * \sa criFs_EndLoadRegion, criFs_BeginGroup
 */
#define criFs_BeginLoadRegion(name)			criFs_BeginGroup(name, NULL)

/*JP
 * \brief ロード区間の終了
 * \ingroup FSLIB_CRIFS
 * \return	CriError 			エラーコード
 * \par 説明:
 * ロード区間の終了を宣言します。
 * \attention
 * 本関数と ::criFs_EndGroup 関数を併用することはできません。<br>
 * （CRI File System Ver.2.02.00より、本関数の機能が ::criFs_EndGroup 関数に統合され、関数自体も ::criFs_EndGroup 関数を呼び出すマクロに変更されました。）<br>
 * \sa criFs_BeginLoadRegion, criFs_EndGroup
 */
#define criFs_EndLoadRegion()				criFs_EndGroup()

/*==========================================================================
 *      CriFsStdio API
 *=========================================================================*/
/**
 * \brief ANSI C に準じたファイルオープン
 * \ingroup FSLIB_CRIFSSTDIO
 * \param[in] bndr  オープンしたいファイルがバインドされているCriFsBinderのハンドル
 * \param[in] fname オープンしたいファイルパス
 * \param[in] mode  オープンモード ("r":読み込み専用モード,"w":書き込み専用モード)
 * \return	CriFsStdioHn 成功した場合、有効なCriFsStdioハンドルを返します。<br>
 *                       失敗した場合はNULLを返します。
 * \par 説明：
 * 指定されたファイルをオープンします。<br>
 * 第一引数には、オープンしたいファイルがバインドされているバインダーを指定します。<br>
 * プラットフォーム標準のファイルパスからファイルをオープンしたい場合、第一引数にはNULLを指定します。<br>
 * 第二引数には、オープンしたいファイルパスを文字列で指定します。<br>
 * 第三引数は、オープンのモードです。"r"を指定すると読み込み専用モード、<br>
 * "w"を指定すると書き込み専用モードでファイルをオープンします。<br>
 * 書き込み専用モードは、ファイル書き込みをサポートしているプラットフォームでのみ正常に動作し、
 * 未サポートのプラットフォームではエラーコールバックが発生し、オープンは失敗します。<br>
 * \par 備考：
 * ファイルの書き込みは以下のルールで行われます。<br>
 * - 指定したファイルが存在しない場合、新規にファイルを作成。
 * - 指定したファイルが既に存在する場合、既存ファイルを編集します。<br>
 * （既存ファイルが削除されることはありません。）
 * <br>
 * 既存ファイルを削除して新規にファイルの書き込みを行いたい場合には、
 * 本関数を実行する前に ::criFsStdio_RemoveFile 関数でファイルの削除を行ってください。
 * \sa criFsStdio_CloseFile, criFsStdio_RemoveFile
 */
CriFsStdioHn CRIAPI criFsStdio_OpenFile(CriFsBinderHn bndr, const char *fname, const char *mode);

/**
 * \brief ANSI C に準じたファイルクローズ
 * \ingroup FSLIB_CRIFSSTDIO
 * \param[in] stdhn クローズするファイルのCriFsStdioハンドル
 * \return CriError エラーコード<br>
 * \par 説明：
 * 指定したファイルをクローズします。<br>
 * 第一引数には、クローズしたいファイルのCriFsStdioハンドルを指定します。<br>
 * \sa criFsStdio_OpenFile
 */
CriError CRIAPI criFsStdio_CloseFile(CriFsStdioHn stdhn);

/*JP
 * \brief ANSI C に準じたAPIに基づくファイルサイズ取得
 * \ingroup FSLIB_CRIFSSTDIO
 * \param[in] stdhn サイズを取得したいファイルのCriFsStdioハンドル
 * \return CriSint64 指定したハンドルが有効であれば、ファイルサイズを返します。
 * \par 説明：
 * 指定したファイルのサイズを取得します。<br>
 * 第一引数には、サイズを取得したいファイルのCriFsStdioハンドルを指定します。<br>
 */
CriSint64 CRIAPI criFsStdio_GetFileSize(CriFsStdioHn stdhn);

/*JP
 * \brief ANSI C に準じたファイルリードオフセットの取得
 * \ingroup FSLIB_CRIFSSTDIO
 * \param[in] stdhn リードオフセットを取得したいファイルのCriFsStdioハンドル
 * \return CriSint64 指定したハンドルが有効であれば、リードオフセット（byte）を返します。
 * \par 説明：
 * 指定したファイルの読込位置を取得します。<br>
 * 第一引数には、読込位置を取得したいファイルの、CriFsStdioハンドルを指定します。<br>
 * \sa criFsStdio_SeekFile
 */
CriSint64 CRIAPI criFsStdio_TellFileOffset(CriFsStdioHn stdhn);

/*JP
 * \brief ANSI C に準じた ファイルリードオフセットのシーク
 * \ingroup FSLIB_CRIFSSTDIO
 * \param[in] rdr                リードオフセットをシークしたいファイルのCriFsStdioハンドル
 * \param[in] offset               シークのオフセット（byte）
 * \param[in] seek_type シーク開始位置の指定
 * \return CriSint64 成功 0<br>
 *                   失敗 -1<br>
 * \par 説明：
 * 指定したファイルのリードオフセットをシークします。<br>
 * 第一引数には、リードオフセットをシークしたいファイルの、CriFsStdioハンドルを指定します。<br>
 * 第二引数には、シークのオフセットを指定します。単位はbyteです。<br>
 * \attention
 * ファイル先頭より手前にシークすることは出来ません。ファイルリードオフセットがファイル先頭より
 * 手前になるようシークオフセットを指定した場合、シーク結果のファイルリードオフセットはファイル先頭になります。<br>
 * 一方、ファイル終端を超えたシークは可能です。<br>
 * また、指定のCriFsStdioハンドルが中間バッファーを持つ場合、
 * 本関数で中間バッファーの有効範囲外にシークすると、中間バッファーの内容が破棄されます。<br>
 * \sa criFsStdio_TellFileOffset
 * \sa criFsStdio_SetInterstageBuffer
 */
CriSint64 CRIAPI criFsStdio_SeekFile(CriFsStdioHn rdr, CriSint64 offset, CRIFSSTDIO_SEEK_TYPE seek_type);

/*JP
 * \brief ANSI C に準じたAPIに基づくファイル読込用中間バッファーの設定
 * \ingroup FSLIB_CRIFSSTDIO
 * \param[in] stdhn            中間バッファーを設定したいCriFsStdioハンドル
 * \param[in] temp_buffer      中間バッファーの先頭アドレス
 * \param[in] temp_buffer_size 中間バッファーのサイズ（byte）
 * \return CriSint64 中間バッファー設定後のファイル読込位置
 * \par 説明：
 * 指定したファイルを読み込む際の中間バッファーを設定します。<br>
 * 第一引数には、中間バッファーを設定したいCriFsStdioハンドルを指定します。<br>
 * 第二引数には、中間バッファーとして利用するメモリ領域の先頭アドレスを指定します。
 * NULLを指定した場合、中間バッファーを使いません。<br>
 * 第三引数には、中間バッファーのサイズを指定します。単位はbyteです。
 * 0を指定した場合、第二引数に有効なアドレスを指定していても、中間バッファーを使いません。<br>
 * \attention
 * criFsStdio_OpenFile()で得られたファイルハンドルは、デフォルトでは中間バッファーを持ちません。<br>
 * 中間バッファーが必要な場合、本関数で設定する必要があります。<br>
 * 中間バッファーを設定すると、最大で temp_buffer_size 分のデータをファイルから
 * 中間バッファーへ読み込むようになります。<br>
 * 中間バッファー内にデータがある限り、
 * criFsStdio_ReadFile()によるファイル読込はメモリコピーになるため、
 * 連続して細かいファイル読込を行う場合
 * 物理的なファイルアクセスの発生頻度を抑えることができます。<br>
 * ただし、criFsStdio_SeekFile()で中間バッファーの有効範囲外にシークすると、
 * 中間バッファーの内容が破棄されます。
 * \sa criFsStdio_TellFileOffset
 * \sa criFsStdio_SeekFile
 * \sa criFsStdio_OpenFile
 * \sa criFsStdio_ReadFile
 */
CriSint64 CRIAPI criFsStdio_SetInterstageBuffer(CriFsStdioHn stdhn, CriUint8 *temp_buffer, CriUint32 temp_buffer_size);

/*JP
 * \brief ANSI C に準じたAPIに基づくファイルからのデータ読込
 * \ingroup FSLIB_CRIFSSTDIO
 * \param[in] stdhn 読込元のCriFsStdioハンドル
 * \param[in] rsize 読込要求サイズ（byte）
 * \param[in] buf   読込先バッファー
 * \param[in] bsize 読込先バッファーのサイズ（byte）
 * \return CriSint64 読込成功 読み込めたサイズ（byte）<br>
 *                   読込失敗 -1
 * \par 説明：
 * ファイルからデータを指定サイズ（byte）分読み込みます。<br>
 * 第一引数には、データの読込元であるファイルのCriFsStdioハンドルを指定します。<br>
 * 第二引数には、読み込むサイズを指定します。<br>
 * 第三引数には、読み込んだデータの書き込み先バッファーを指定します。<br>
 * 第四引数には、読み込んだデータの書き込み先バッファーサイズを指定します。<br>
 * \attention
 * 戻り値は、常に<b>読込要求サイズ以下になる</b>ことに注意してください。<br>
 * 例えばファイル終端では、戻り値が読込要求サイズより小さくなることがありますが、
 * 読込に失敗しているわけではありません。読込に失敗した場合、-1を返します。
 */
CriSint64 CRIAPI criFsStdio_ReadFile(CriFsStdioHn stdhn, CriSint64 rsize, void *buf, CriSint64 bsize);

/*JP
 * \brief ANSI C に準じたAPIに基づくデータのファイル書込
 * \ingroup FSLIB_CRIFSSTDIO
 * \param[in] stdhn 書込先のCriFsStdioハンドル
 * \param[in] rsize 書込要求サイズ（byte）
 * \param[in] buf   書込元バッファー
 * \param[in] bsize 書込元バッファーのサイズ（byte）
 * \return CriSint64 書込成功 書き込めたサイズ（byte）<br>
 *                   書込失敗 -1
 * \par 説明：
 * ファイルからデータを指定サイズ（byte）分読み込みます。<br>
 * 第一引数には、データの書込先であるファイルのCriFsStdioハンドルを指定します。<br>
 * 第二引数には、書き込むサイズを指定します。<br>
 * 第三引数には、書込元となるデータのバッファーを指定します。<br>
 * 第四引数には、書込元となるデータのバッファーサイズを指定します。<br>
 * \attention
 * 戻り値は、書込に成功したサイズ（byte）です。<br>
 * 書込に失敗した場合、-1を返します。
 * 本関数は、ファイル書き込みをサポートしているプラットフォームにおいてのみ使用することができます。<br>
 * ファイル書き込みをサポートしていないプラットフォームで本関数を呼び出すと、<br>
 * リンク時にシンボルが見つからず、ビルドエラーになります。
 */
CriSint64 CRIAPI criFsStdio_WriteFile(CriFsStdioHn stdhn, CriSint64 rsize, void *buf, CriSint64 bsize);

/**
 * \brief CriFsStdioハンドルのファイル読み込みプライオリティを変更
 * \ingroup FSLIB_CRIFSSTDIO
 * \param[in] stdhn プライオリティを変更したいCriFsStdioハンドル
 * \param[in] prio  変更したいプライオリティの値
 * \return CriError CRIERR_OK 成功<br>
 *                  その他   失敗
 * \par 説明：
 * criFsStdio_ReadFile()を使ったファイル読み込み優先度を、CriFsStdioハンドル毎に設定します。<br>
 */
CriError CRIAPI criFsStdio_SetReadPriority(CriFsStdioHn stdhn, CriFsLoaderPriority prio);

/**
 * \brief ファイルの削除
 * \ingroup FSLIB_CRIFSSTDIO
 * \param[in]	binder		削除したいファイルがバインドされているCriFsBinderのハンドル
 * \param[in]	path		削除するファイルのパス
 * \param[out]	result		削除結果
 * \return		CriError	エラーコード
 * \par 説明：
 * 指定したファイルを削除します。<br>
 * ファイルの削除に成功した場合、出力値（ result ）に CRIFSSTDIO_FILE_REMOVED がセットされます。<br>
 * 指定したファイルが存在しない場合や、読み込み専用ファイルを削除しようとした場合、
 * ファイルの削除に失敗し、出力値（ result ）に CRIFSSTDIO_IO_ERROR_OCCURRD がセットされます。<br>
 * 本関数は、ファイル書き込みをサポートしているプラットフォームにおいてのみ使用することができます。<br>
 * \par 備考：
 * 指定した引数が不正な場合や、ファイル削除用のハンドルが確保できない場合、
 * 出力値（ result ）に CRIFSSTDIO_NOT_EXECUTED がセットされ、
 * 関数の戻り値がエラー値（ CRIERR_OK 以外の値）になります。<br>
 */
CriError CRIAPI criFsStdio_RemoveFile(
	CriFsBinderHn binder, const CriChar8 *path, CriFsStdioRemoveResult *result);

#ifdef __cplusplus
}
#endif

/***************************************************************************
 *      旧バージョンとの互換用
 *      For compatibility with old version
 ***************************************************************************/
#define CRI_FS_VERSION		(0xFFFFFFFF)
#define CriFsConfiguration				CriFsConfig
#define criFs_InitializeConfiguration(config)	criFs_SetDefaultConfig(&config)
#define criFs_CalculateWorkSize(config, nbyte)	criFs_CalculateWorkSizeForLibrary(&config, nbyte)
#define criFs_Initialize(config, buffer, size)	criFs_InitializeLibrary(&config, buffer, size)
#define criFs_Finalize()				criFs_FinalizeLibrary()
#define CriFsBinderUserHeapAllocateCbFunc CriFsMallocFunc
#define CriFsBinderUserHeapFreeCbFunc CriFsFreeFunc
#define CRIFS_PRIMARYCPK_ERROR_NONE	(CRIFS_BINDCPK_ERROR_NONE)
#define CRIFS_PRIMARYCPK_ERROR_CRC  (CRIFS_BINDCPK_ERROR_DATA)
#define CRIFS_PRIMARYCPK_ERROR_CANNOT_READ	(CRIFS_BINDCPK_ERROR_CANNOT_READ)
#define CRIFS_PRIMARYCPK_ERROR_NONEXISTENT	(CRIFS_BINDCPK_ERROR_NONEXISTENT)
typedef CriFsBindCpkError CriFsPrimaryCpkError;
typedef CriFsBindId CriFsBinderId;

#endif	/* CRI_FILE_SYSTEM_H_INCLUDED */

/* --- end of file --- */
