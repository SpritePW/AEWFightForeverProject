/****************************************************************************
 *
 * CRI Middleware SDK
 *
 * Copyright (c) 2006-2011 CRI Middleware Co., Ltd.
 *
 * Library  : CRI File System
 * Module   : Library User's Header for PC
 * File     : cri_file_system_pc.h
 *
 ****************************************************************************/
/*!
 *	\file		cri_file_system_pc.h
 */

/* 多重定義防止					*/
/* Prevention of redefinition	*/
#ifndef	CRI_FILE_SYSTEM_PC_H_INCLUDED
#define	CRI_FILE_SYSTEM_PC_H_INCLUDED

/***************************************************************************
 *      インクルードファイル
 *      Include files
 ***************************************************************************/
#include <windows.h>
#include "cri_le_xpt.h"
#include "cri_le_error.h"

/***************************************************************************
 *      定数マクロ
 *      Macro Constants
 ***************************************************************************/

/***************************************************************************
 *      処理マクロ
 *      Macro Functions
 ***************************************************************************/

/***************************************************************************
 *      データ型宣言
 *      Data Type Declarations
 ***************************************************************************/

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

/*JP
 * \brief サーバー処理スレッドのプライオリティ設定
 * \ingroup FSLIB_CRIFS_PC
 * \param[in]	prio	スレッドのプライオリティ
 * \return	CriError	エラーコード
 * \par 説明:
 * サーバー処理（ライブラリの内部処理）を行うスレッドのプライオリティを設定します。<BR>
 * 引数 prio には Win32 API のスレッド優先レベル（ SetThreadPriority 関数の引数）を指定します。<BR>
 * アプリケーションのメインスレッドよりも高いプライオリティを指定してください。<BR>
 * プライオリティのデフォルト値は THREAD_PRIORITY_HIGHEST です。<BR>
 * \attention:
 * 本関数は、ライブラリ初期化時にスレッドモデルをマルチスレッドモデル
 * （ ::CRIFS_THREAD_MODEL_MULTI ）に設定した場合にのみ効果を発揮します。<br>
 * 他のスレッドモデルを選択した場合、本関数は何も処理を行いません。<br>
 * （エラーコールバックが発生します。）<br>
 * <br>
 * 本関数は初期化後〜終了処理前の間に実行する必要があります。<br>
 * 初期化前や終了処理後に本関数を実行しても、効果はありません。<br>
 * （エラーコールバックが発生します。）<br>
 * <br>
 * サーバー処理スレッドは、CRI Atomライブラリで使用しているものと同じです（共用しています）。<br>
 * すでにCRI AtomライブラリのAPIでサーバー処理スレッドの設定を変更している場合、<br>
 * 本関数を呼び出すと設定を上書きしてしまうのでご注意ください。
 */
CriError CRIAPI criFs_SetServerThreadPriority_PC(int prio);

/*JP
 * \brief サーバー処理スレッドのプライオリティ取得
 * \ingroup FSLIB_CRIFS_PC
 * \param[out]	prio	スレッドのプライオリティ
 * \return	CriError	エラーコード
 * \par 説明:
 * サーバー処理（ライブラリの内部処理）を行うスレッドのプライオリティを取得します。<BR>
 * \attention:
 * 本関数は、ライブラリ初期化時にスレッドモデルをマルチスレッドモデル
 * （ ::CRIFS_THREAD_MODEL_MULTI ）に設定した場合にのみ効果を発揮します。<br>
 * 他のスレッドモデルを選択した場合、本関数はエラーを返します。<br>
 * <br>
 * 本関数は初期化後〜終了処理前の間に実行する必要があります。<br>
 * 初期化前や終了処理後に本関数を実行した場合、本関数はエラーを返します。<br>
 */
CriError CRIAPI criFs_GetServerThreadPriority_PC(int *prio);

/*JP
 * \brief ファイルアクセススレッドのプライオリティ設定
 * \ingroup FSLIB_CRIFS_PC
 * \param[in]	prio	スレッドのプライオリティ
 * \return	CriError	エラーコード
 * \par 説明:
 * ファイルアクセススレッドのプライオリティを設定します。<BR>
 * 引数 prio には Win32 API のスレッド優先レベル（ SetThreadPriority 関数の引数）を指定します。<BR>
 * アプリケーションのメインスレッドよりも高いプライオリティを指定してください。<BR>
 * プライオリティのデフォルト値は THREAD_PRIORITY_HIGHEST です。<BR>
 * \attention:
 * 本関数は、ライブラリ初期化時にスレッドモデルをマルチスレッドモデル
 * （ ::CRIFS_THREAD_MODEL_MULTI ）に設定した場合にのみ効果を発揮します。<br>
 * 他のスレッドモデルを選択した場合、本関数は何も処理を行いません。<br>
 * （エラーコールバックが発生します。）<br>
 * <br>
 * 本関数は初期化後〜終了処理前の間に実行する必要があります。<br>
 * 初期化前や終了処理後に本関数を実行しても、効果はありません。<br>
 * （エラーコールバックが発生します。）<br>
 */
CriError CRIAPI criFs_SetFileAccessThreadPriority_PC(int prio);

/*JP
 * \brief ファイルアクセススレッドのプライオリティ取得
 * \ingroup FSLIB_CRIFS_PC
 * \param[out]	prio	スレッドのプライオリティ
 * \return	CriError	エラーコード
 * \par 説明:
 * ファイルアクセススレッドのプライオリティを取得します。<BR>
 * \attention:
 * 本関数は、ライブラリ初期化時にスレッドモデルをマルチスレッドモデル
 * （ ::CRIFS_THREAD_MODEL_MULTI ）に設定した場合にのみ効果を発揮します。<br>
 * 他のスレッドモデルを選択した場合、本関数はエラーを返します。<br>
 * <br>
 * 本関数は初期化後〜終了処理前の間に実行する必要があります。<br>
 * 初期化前や終了処理後に本関数を実行した場合、本関数はエラーを返します。<br>
 */
CriError CRIAPI criFs_GetFileAccessThreadPriority_PC(int *prio);

/*JP
 * \brief メモリファイルシステムスレッドのプライオリティ設定
 * \ingroup FSLIB_CRIFS_PC
 * \param[in]	prio	スレッドのプライオリティ
 * \return	CriError	エラーコード
 * \par 説明:
 * メモリファイルシステムスレッドのプライオリティを設定します。<BR>
 * 引数 prio には Win32 API のスレッド優先レベル（ SetThreadPriority 関数の引数）を指定します。<BR>
 * アプリケーションのメインスレッドよりも低いプライオリティを指定してください。<BR>
 * プライオリティのデフォルト値は THREAD_PRIORITY_LOWEST です。<BR>
 * \attention:
 * 本関数は、ライブラリ初期化時にスレッドモデルをマルチスレッドモデル
 * （ ::CRIFS_THREAD_MODEL_MULTI ）に設定した場合にのみ効果を発揮します。<br>
 * 他のスレッドモデルを選択した場合、本関数は何も処理を行いません。<br>
 * （エラーコールバックが発生します。）<br>
 * <br>
 * 本関数は初期化後〜終了処理前の間に実行する必要があります。<br>
 * 初期化前や終了処理後に本関数を実行しても、効果はありません。<br>
 * （エラーコールバックが発生します。）<br>
 */
CriError CRIAPI criFs_SetMemoryFileSystemThreadPriority_PC(int prio);

/*JP
 * \brief メモリファイルシステムスレッドのプライオリティ取得
 * \ingroup FSLIB_CRIFS_PC
 * \param[out]	prio	スレッドのプライオリティ
 * \return	CriError	エラーコード
 * \par 説明:
 * メモリファイルシステムスレッドのプライオリティを取得します。<BR>
 * \attention:
 * 本関数は、ライブラリ初期化時にスレッドモデルをマルチスレッドモデル
 * （ ::CRIFS_THREAD_MODEL_MULTI ）に設定した場合にのみ効果を発揮します。<br>
 * 他のスレッドモデルを選択した場合、本関数はエラーを返します。<br>
 * <br>
 * 本関数は初期化後〜終了処理前の間に実行する必要があります。<br>
 * 初期化前や終了処理後に本関数を実行した場合、本関数はエラーを返します。<br>
 */
CriError CRIAPI criFs_GetMemoryFileSystemThreadPriority_PC(int *prio);

/*JP
 * \brief データ展開スレッドのプライオリティ設定
 * \ingroup FSLIB_CRIFS_PC
 * \param[in]	prio	スレッドのプライオリティ
 * \return	CriError	エラーコード
 * \par 説明:
 * データ展開スレッドのプライオリティを設定します。<BR>
 * 引数 prio には Win32 API のスレッド優先レベル（ SetThreadPriority 関数の引数）を指定します。<BR>
 * アプリケーションのメインスレッドよりも低いプライオリティを指定してください。<BR>
 * プライオリティのデフォルト値は THREAD_PRIORITY_LOWEST です。<BR>
 * \attention:
 * 本関数は、ライブラリ初期化時にスレッドモデルをマルチスレッドモデル
 * （ ::CRIFS_THREAD_MODEL_MULTI ）に設定した場合にのみ効果を発揮します。<br>
 * 他のスレッドモデルを選択した場合、本関数は何も処理を行いません。<br>
 * （エラーコールバックが発生します。）<br>
 * <br>
 * 本関数は初期化後〜終了処理前の間に実行する必要があります。<br>
 * 初期化前や終了処理後に本関数を実行しても、効果はありません。<br>
 * （エラーコールバックが発生します。）<br>
 */
CriError CRIAPI criFs_SetDataDecompressionThreadPriority_PC(int prio);

/*JP
 * \brief データ展開スレッドのプライオリティ取得
 * \ingroup FSLIB_CRIFS_PC
 * \param[out]	prio	スレッドのプライオリティ
 * \return	CriError	エラーコード
 * \par 説明:
 * データ展開スレッドのプライオリティを取得します。<BR>
 * \attention:
 * 本関数は、ライブラリ初期化時にスレッドモデルをマルチスレッドモデル
 * （ ::CRIFS_THREAD_MODEL_MULTI ）に設定した場合にのみ効果を発揮します。<br>
 * 他のスレッドモデルを選択した場合、本関数はエラーを返します。<br>
 * <br>
 * 本関数は初期化後〜終了処理前の間に実行する必要があります。<br>
 * 初期化前や終了処理後に本関数を実行した場合、本関数はエラーを返します。<br>
 */
CriError CRIAPI criFs_GetDataDecompressionThreadPriority_PC(int *prio);

/*JP
 * \brief インストーラースレッドのプライオリティ設定
 * \ingroup FSLIB_CRIFS_PC
 * \param[in]	prio	スレッドのプライオリティ
 * \return	CriError	エラーコード
 * \par 説明:
 * インストーラースレッドのプライオリティを設定します。<BR>
 * 引数 prio には Win32 API のスレッド優先レベル（ SetThreadPriority 関数の引数）を指定します。<BR>
 * アプリケーションのメインスレッドよりも高いプライオリティを指定してください。<BR>
 * プライオリティのデフォルト値は THREAD_PRIORITY_ABOVE_NORMAL です。<BR>
 * \attention:
 * 本関数は、ライブラリ初期化時にスレッドモデルをマルチスレッドモデル
 * （ ::CRIFS_THREAD_MODEL_MULTI ）に設定した場合にのみ効果を発揮します。<br>
 * 他のスレッドモデルを選択した場合、本関数は何も処理を行いません。<br>
 * （エラーコールバックが発生します。）<br>
 * <br>
 * 本関数は初期化後〜終了処理前の間に実行する必要があります。<br>
 * 初期化前や終了処理後に本関数を実行しても、効果はありません。<br>
 * （エラーコールバックが発生します。）<br>
 */
CriError CRIAPI criFs_SetInstallerThreadPriority_PC(int prio);

/*JP
 * \brief インストーラースレッドのプライオリティ取得
 * \ingroup FSLIB_CRIFS_PC
 * \param[out]	prio	スレッドのプライオリティ
 * \return	CriError	エラーコード
 * \par 説明:
 * インストーラースレッドのプライオリティを取得します。<BR>
 * \attention:
 * 本関数は、ライブラリ初期化時にスレッドモデルをマルチスレッドモデル
 * （ ::CRIFS_THREAD_MODEL_MULTI ）に設定した場合にのみ効果を発揮します。<br>
 * 他のスレッドモデルを選択した場合、本関数はエラーを返します。<br>
 * <br>
 * 本関数は初期化後〜終了処理前の間に実行する必要があります。<br>
 * 初期化前や終了処理後に本関数を実行した場合、本関数はエラーを返します。<br>
 */
CriError CRIAPI criFs_GetInstallerThreadPriority_PC(int *prio);

/*JP
 * \brief サーバー処理スレッドのアフィニティマスク設定
 * \ingroup FSLIB_CRIFS_PC
 * \param[in]	mask	アフィニティマスク
 * \return	CriError	エラーコード
 * \par 説明:
 * サーバー処理（ライブラリの内部処理）を行うスレッドのアフィニティマスクを設定します。<BR>
 * 引数 mask には Win32 API のスレッドアフィニティマスク（ SetThreadAffinityMask 関数の引数）を指定します。<BR>
 * デフォルト状態ではアフィニティマスクは設定されていません。<BR>
 * \attention:
 * 本関数は、ライブラリ初期化時にスレッドモデルをマルチスレッドモデル
 * （ ::CRIFS_THREAD_MODEL_MULTI ）に設定した場合にのみ効果を発揮します。<br>
 * 他のスレッドモデルを選択した場合、本関数は何も処理を行いません。<br>
 * （エラーコールバックが発生します。）<br>
 * <br>
 * 本関数は初期化後〜終了処理前の間に実行する必要があります。<br>
 * 初期化前や終了処理後に本関数を実行しても、効果はありません。<br>
 * （エラーコールバックが発生します。）<br>
 * <br>
 * サーバー処理スレッドは、CRI Atomライブラリで使用しているものと同じです（共用しています）。<br>
 * すでにCRI AtomライブラリのAPIでサーバー処理スレッドの設定を変更している場合、<br>
 * 本関数を呼び出すと設定を上書きしてしまうのでご注意ください。
 */
CriError CRIAPI criFs_SetServerThreadAffinityMask_PC(DWORD_PTR mask);

/*JP
 * \brief サーバー処理スレッドのアフィニティマスク取得
 * \ingroup FSLIB_CRIFS_PC
 * \param[out]	mask	アフィニティマスク
 * \return	CriError	エラーコード
 * \par 説明:
 * サーバー処理（ライブラリの内部処理）を行うスレッドのアフィニティマスクを取得します。<BR>
 * \attention:
 * 本関数は、ライブラリ初期化時にスレッドモデルをマルチスレッドモデル
 * （ ::CRIFS_THREAD_MODEL_MULTI ）に設定した場合にのみ効果を発揮します。<br>
 * 他のスレッドモデルを選択した場合、本関数はエラーを返します。<br>
 * <br>
 * 本関数は初期化後〜終了処理前の間に実行する必要があります。<br>
 * 初期化前や終了処理後に本関数を実行した場合、本関数はエラーを返します。<br>
 */
CriError CRIAPI criFs_GetServerThreadAffinityMask_PC(DWORD_PTR *mask);

/*JP
 * \brief ファイルアクセススレッドのアフィニティマスク設定
 * \ingroup FSLIB_CRIFS_PC
 * \param[in]	mask	アフィニティマスク
 * \return	CriError	エラーコード
 * \par 説明:
 * ファイルアクセススレッドのアフィニティマスクを設定します。<BR>
 * 引数 mask には Win32 API のスレッドアフィニティマスク（ SetThreadAffinityMask 関数の引数）を指定します。<BR>
 * デフォルト状態ではアフィニティマスクは設定されていません。<BR>
 * \attention:
 * 本関数は、ライブラリ初期化時にスレッドモデルをマルチスレッドモデル
 * （ ::CRIFS_THREAD_MODEL_MULTI ）に設定した場合にのみ効果を発揮します。<br>
 * 他のスレッドモデルを選択した場合、本関数は何も処理を行いません。<br>
 * （エラーコールバックが発生します。）<br>
 * <br>
 * 本関数は初期化後〜終了処理前の間に実行する必要があります。<br>
 * 初期化前や終了処理後に本関数を実行しても、効果はありません。<br>
 * （エラーコールバックが発生します。）<br>
 */
CriError CRIAPI criFs_SetFileAccessThreadAffinityMask_PC(DWORD_PTR mask);

/*JP
 * \brief ファイルアクセススレッドのアフィニティマスク取得
 * \ingroup FSLIB_CRIFS_PC
 * \param[out]	mask	アフィニティマスク
 * \return	CriError	エラーコード
 * \par 説明:
 * ファイルアクセススレッドのアフィニティマスクを取得します。<BR>
 * \attention:
 * 本関数は、ライブラリ初期化時にスレッドモデルをマルチスレッドモデル
 * （ ::CRIFS_THREAD_MODEL_MULTI ）に設定した場合にのみ効果を発揮します。<br>
 * 他のスレッドモデルを選択した場合、本関数はエラーを返します。<br>
 * <br>
 * 本関数は初期化後〜終了処理前の間に実行する必要があります。<br>
 * 初期化前や終了処理後に本関数を実行した場合、本関数はエラーを返します。<br>
 */
CriError CRIAPI criFs_GetFileAccessThreadAffinityMask_PC(DWORD_PTR *mask);

/*JP
 * \brief メモリファイルシステムスレッドのアフィニティマスク設定
 * \ingroup FSLIB_CRIFS_PC
 * \param[in]	mask	アフィニティマスク
 * \return	CriError	エラーコード
 * \par 説明:
 * メモリファイルシステムスレッドのアフィニティマスクを設定します。<BR>
 * 引数 mask には Win32 API のスレッドアフィニティマスク（ SetThreadAffinityMask 関数の引数）を指定します。<BR>
 * デフォルト状態ではアフィニティマスクは設定されていません。<BR>
 * \attention:
 * 本関数は、ライブラリ初期化時にスレッドモデルをマルチスレッドモデル
 * （ ::CRIFS_THREAD_MODEL_MULTI ）に設定した場合にのみ効果を発揮します。<br>
 * 他のスレッドモデルを選択した場合、本関数は何も処理を行いません。<br>
 * （エラーコールバックが発生します。）<br>
 * <br>
 * 本関数は初期化後〜終了処理前の間に実行する必要があります。<br>
 * 初期化前や終了処理後に本関数を実行しても、効果はありません。<br>
 * （エラーコールバックが発生します。）<br>
 */
CriError CRIAPI criFs_SetMemoryFileSystemThreadAffinityMask_PC(DWORD_PTR mask);

/*JP
 * \brief メモリファイルシステムスレッドのアフィニティマスク取得
 * \ingroup FSLIB_CRIFS_PC
 * \param[out]	mask	アフィニティマスク
 * \return	CriError	エラーコード
 * \par 説明:
 * メモリファイルシステムスレッドのアフィニティマスクを取得します。<BR>
 * \attention:
 * 本関数は、ライブラリ初期化時にスレッドモデルをマルチスレッドモデル
 * （ ::CRIFS_THREAD_MODEL_MULTI ）に設定した場合にのみ効果を発揮します。<br>
 * 他のスレッドモデルを選択した場合、本関数はエラーを返します。<br>
 * <br>
 * 本関数は初期化後〜終了処理前の間に実行する必要があります。<br>
 * 初期化前や終了処理後に本関数を実行した場合、本関数はエラーを返します。<br>
 */
CriError CRIAPI criFs_GetMemoryFileSystemThreadAffinityMask_PC(DWORD_PTR *mask);

/*JP
 * \brief データ展開スレッドのアフィニティマスク設定
 * \ingroup FSLIB_CRIFS_PC
 * \param[in]	mask	アフィニティマスク
 * \return	CriError	エラーコード
 * \par 説明:
 * データ展開スレッドのアフィニティマスクを設定します。<BR>
 * 引数 mask には Win32 API のスレッドアフィニティマスク（ SetThreadAffinityMask 関数の引数）を指定します。<BR>
 * デフォルト状態ではアフィニティマスクは設定されていません。<BR>
 * \attention:
 * 本関数は、ライブラリ初期化時にスレッドモデルをマルチスレッドモデル
 * （ ::CRIFS_THREAD_MODEL_MULTI ）に設定した場合にのみ効果を発揮します。<br>
 * 他のスレッドモデルを選択した場合、本関数は何も処理を行いません。<br>
 * （エラーコールバックが発生します。）<br>
 * <br>
 * 本関数は初期化後〜終了処理前の間に実行する必要があります。<br>
 * 初期化前や終了処理後に本関数を実行しても、効果はありません。<br>
 * （エラーコールバックが発生します。）<br>
 */
CriError CRIAPI criFs_SetDataDecompressionThreadAffinityMask_PC(DWORD_PTR mask);

/*JP
 * \brief データ展開スレッドのアフィニティマスク取得
 * \ingroup FSLIB_CRIFS_PC
 * \param[out]	mask	アフィニティマスク
 * \return	CriError	エラーコード
 * \par 説明:
 * データ展開スレッドのアフィニティマスクを取得します。<BR>
 * \attention:
 * 本関数は、ライブラリ初期化時にスレッドモデルをマルチスレッドモデル
 * （ ::CRIFS_THREAD_MODEL_MULTI ）に設定した場合にのみ効果を発揮します。<br>
 * 他のスレッドモデルを選択した場合、本関数はエラーを返します。<br>
 * <br>
 * 本関数は初期化後〜終了処理前の間に実行する必要があります。<br>
 * 初期化前や終了処理後に本関数を実行した場合、本関数はエラーを返します。<br>
 */
CriError CRIAPI criFs_GetDataDecompressionThreadAffinityMask_PC(DWORD_PTR *mask);

/*JP
 * \brief インストーラースレッドのアフィニティマスク設定
 * \ingroup FSLIB_CRIFS_PC
 * \param[in]	mask	アフィニティマスク
 * \return	CriError	エラーコード
 * \par 説明:
 * インストーラースレッドのアフィニティマスクを設定します。<BR>
 * 引数 mask には Win32 API のスレッドアフィニティマスク（ SetThreadAffinityMask 関数の引数）を指定します。<BR>
 * デフォルト状態ではアフィニティマスクは設定されていません。<BR>
 * \attention:
 * 本関数は、ライブラリ初期化時にスレッドモデルをマルチスレッドモデル
 * （ ::CRIFS_THREAD_MODEL_MULTI ）に設定した場合にのみ効果を発揮します。<br>
 * 他のスレッドモデルを選択した場合、本関数は何も処理を行いません。<br>
 * （エラーコールバックが発生します。）<br>
 * <br>
 * 本関数は初期化後〜終了処理前の間に実行する必要があります。<br>
 * 初期化前や終了処理後に本関数を実行しても、効果はありません。<br>
 * （エラーコールバックが発生します。）<br>
 */
CriError CRIAPI criFs_SetInstallerThreadAffinityMask_PC(DWORD_PTR mask);

/*JP
 * \brief インストーラースレッドのアフィニティマスク取得
 * \ingroup FSLIB_CRIFS_PC
 * \param[out]	mask	アフィニティマスク
 * \return	CriError	エラーコード
 * \par 説明:
 * インストーラースレッドのアフィニティマスクを取得します。<BR>
 * \attention:
 * 本関数は、ライブラリ初期化時にスレッドモデルをマルチスレッドモデル
 * （ ::CRIFS_THREAD_MODEL_MULTI ）に設定した場合にのみ効果を発揮します。<br>
 * 他のスレッドモデルを選択した場合、本関数はエラーを返します。<br>
 * <br>
 * 本関数は初期化後〜終了処理前の間に実行する必要があります。<br>
 * 初期化前や終了処理後に本関数を実行した場合、本関数はエラーを返します。<br>
 */
CriError CRIAPI criFs_GetInstallerThreadAffinityMask_PC(DWORD_PTR *mask);

/*JP
 * \brief パスの文字エンコーディングタイプを Unicode から UTF-8 に切り替え
 * \ingroup FSLIB_CRIFS_PC
 * \param[in]	sw		文字列エンコーディングタイプを UTF-8 にするかどうか
 * \return	CriError	エラーコード
 * \par 説明:
 * アプリケーションからUTF-8のファイルパスを指定したい場合、本関数をご利用ください。<br>
 * デフォルトでは、ファイルパス文字列をマルチバイト文字セットとして取り扱います。(CreateFileAを使用)<br>
 * そのため、実際にシステムが解釈する文字エンコーディングはシステムロケールに依存します。<br>
 * <br>
 * 本関数を使用すると、ファイルパスをUnicodeとして取り扱うように変更することができます。(CreateFileWを使用)<br>
 * 引数に CRI_TRUE を指定して実行すると、ファイルパスはUTF-8としてみなし、ライブラリ内部でUTF-16に変換してファイルオープンを行います。
 */
CriError CRIAPI criFs_SwitchPathUnicodeToUtf8_PC(CriBool sw);

#ifdef __cplusplus
}
#endif

#endif	/* CRI_FILE_SYSTEM_PC_H_INCLUDED */

/* --- end of file --- */
