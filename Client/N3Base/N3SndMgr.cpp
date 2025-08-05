// N3SndMgr.cpp: implementation of the CN3SndMgr class.
//
//////////////////////////////////////////////////////////////////////
#include "StdAfxBase.h"
#include "N3SndMgr.h"
#include "N3SndObj.h"
#include "N3SndObjStream.h"
#include "N3Base.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CN3SndMgr::CN3SndMgr()
{
	m_bSndEnable = false;	
	m_bSndDuplicated = false;
}

CN3SndMgr::~CN3SndMgr()
{
	Release();
}

//
//	엔진 초기화..
//
void CN3SndMgr::Init(HWND hWnd)
{
	Release();
	m_bSndEnable = CN3SndObj::StaticInit(hWnd);
	m_Tbl_Source.LoadFromFile("Data\\sound.tbl");
}

CN3SndObj* CN3SndMgr::CreateObj(int iID, e_SndType eType)
{
	TABLE_SOUND* pTbl = m_Tbl_Source.Find(iID);
	if(pTbl==NULL) return NULL;

	return this->CreateObj(pTbl->szFN, eType);
}

void CN3SndMgr::CleanupTempFiles()
{
	for(const auto& path : m_TempFilePaths)
	{
		remove(path.c_str());
	}
	m_TempFilePaths.clear();
}

// Helper to check if the file is an MP3
bool IsMP3File(const std::string& szFN)
{
	if(szFN.length() < 4) return false;
	return (_stricmp(szFN.substr(szFN.length() - 4).c_str(), ".mp3") == 0);
}

// MP3 decoding function
bool DecodeMP3ToFile(const std::string& szMP3Path, std::string& szWAVPath)
{
	mpg123_handle* mh;
	unsigned char* buffer;
	size_t buffer_size;
	size_t done;
	int err;

	int channels, encoding;
	long rate;

	// Create a temporary file path
	char tempPath[MAX_PATH];
	GetTempPath(MAX_PATH, tempPath);
	char tempFile[MAX_PATH];
	GetTempFileName(tempPath, "wav", 0, tempFile);
	szWAVPath = tempFile;

	FILE* wavFile = fopen(szWAVPath.c_str(), "wb");
	if(!wavFile)
		return false;

	/* initializations */
	mpg123_init();
	mh = mpg123_new(NULL, &err);
	if(mh == NULL)
	{
		fprintf(stderr, "Error initializing mpg123: %s\n", mpg123_plain_strerror(err));
		fclose(wavFile);
		return false;
	}

	/* open the file and get the decoding format */
	if(mpg123_open(mh, szMP3Path.c_str()) != MPG123_OK)
	{
		fprintf(stderr, "Error opening file: %s\n", mpg123_strerror(mh));
		mpg123_delete(mh);
		fclose(wavFile);
		return false;
	}

	if(mpg123_getformat(mh, &rate, &channels, &encoding) != MPG123_OK)
	{
		fprintf(stderr, "Error getting format: %s\n", mpg123_strerror(mh));
		mpg123_delete(mh);
		fclose(wavFile);
		return false;
	}

	// Write WAV header
	short bits = mpg123_encsize(encoding) * 8;
	short block_align = (bits / 8) * channels;
	int byte_rate = rate * block_align;
	int data_size = 0;

	// RIFF chunk
	fwrite("RIFF", 1, 4, wavFile);
	int riff_size = 0; // Placeholder
	fwrite(&riff_size, 4, 1, wavFile);
	fwrite("WAVE", 1, 4, wavFile);

	// fmt chunk
	fwrite("fmt ", 1, 4, wavFile);
	int fmt_size = 16;
	fwrite(&fmt_size, 4, 1, wavFile);
	short audio_format = 1; // PCM
	fwrite(&audio_format, 2, 1, wavFile);
	fwrite(&channels, 2, 1, wavFile);
	fwrite(&rate, 4, 1, wavFile);
	fwrite(&byte_rate, 4, 1, wavFile);
	fwrite(&block_align, 2, 1, wavFile);
	fwrite(&bits, 2, 1, wavFile);

	// data chunk
	fwrite("data", 1, 4, wavFile);
	fwrite(&data_size, 4, 1, wavFile); // Placeholder

	buffer_size = mpg123_outblock(mh);
	buffer = (unsigned char*)malloc(buffer_size * sizeof(unsigned char));

	/* decode and write to WAV */
	while(mpg123_read(mh, buffer, buffer_size, &done) == MPG123_OK)
	{
		fwrite(buffer, 1, done, wavFile);
		data_size += done;
	}

	// Go back and fill in the size fields in the header
	fseek(wavFile, 4, SEEK_SET);
	riff_size = data_size + 36;
	fwrite(&riff_size, 4, 1, wavFile);
	fseek(wavFile, 40, SEEK_SET);
	fwrite(&data_size, 4, 1, wavFile);

	/* clean up */
	free(buffer);
	fclose(wavFile);
	mpg123_close(mh);
	mpg123_delete(mh);
	mpg123_exit();

	return true;
}

CN3SndObj* CN3SndMgr::CreateObj(const std::string& szFN, e_SndType eType)
{
	if(!m_bSndEnable) return NULL;

	std::string finalPath = szFN;
	bool isMP3 = IsMP3File(szFN);
	if(isMP3)
	{
		std::string wavPath;
		if (DecodeMP3ToFile(szFN, wavPath))
		{
			finalPath = wavPath;
			m_TempFilePaths.push_back(wavPath);
		}
		else
		{
			// Handle decoding failure, maybe log an error
			return NULL;
		}
	}

	CN3SndObj* pObjSrc = NULL;
	itm_Snd it = m_SndObjSrcs.find(finalPath);
	if(it == m_SndObjSrcs.end()) // 못 찾았다... 새로 만들자..
	{
		pObjSrc = new CN3SndObj();
		if(false == pObjSrc->Create(finalPath, eType)) // 새로 로딩..
		{
			delete pObjSrc; pObjSrc = NULL;
			return NULL;
		}
		m_SndObjSrcs.insert(val_Snd(finalPath, pObjSrc)); // 맵에 추가한다..
	}
	else pObjSrc = it->second;

	if(!m_bSndDuplicated) return pObjSrc;//this_Snd

	if(NULL == pObjSrc) return NULL;

	CN3SndObj* pObjNew = new CN3SndObj();
	if(false == pObjNew->Duplicate(pObjSrc, eType)) // Duplicate 처리..
	{
		delete pObjNew; pObjNew = NULL;
		return NULL;
	}
	if(pObjNew) m_SndObjs_Duplicated.push_back(pObjNew); // 리스트에 넣는다...
	return pObjNew;
}

CN3SndObjStream* CN3SndMgr::CreateStreamObj(const std::string& szFN)
{
	if(!m_bSndEnable) return NULL;

	std::string finalPath = szFN;
	bool isMP3 = IsMP3File(szFN);
	if(isMP3)
	{
		std::string fullpath = CN3Base::PathGet();
		std::string wavPath;
		if(DecodeMP3ToFile(fullpath+szFN, wavPath))
		{
			finalPath = wavPath;
			m_TempFilePaths.push_back(wavPath);
		}
		else
		{
			// Handle decoding failure
			return NULL;
		}
	}

	CN3SndObjStream* pObj = new CN3SndObjStream();
	if(false == pObj->Create(finalPath))
	{
		delete pObj; pObj = NULL;
		return NULL;
	}

	m_SndObjStreams.push_back(pObj); // 리스트에 넣기..

	return pObj;
}

CN3SndObjStream* CN3SndMgr::CreateStreamObj(int iID)
{
	TABLE_SOUND* pTbl = m_Tbl_Source.Find(iID);
	if(pTbl==NULL) return NULL;

	return this->CreateStreamObj(pTbl->szFN);
}

void CN3SndMgr::ReleaseStreamObj(CN3SndObjStream** ppObj)
{
	if(NULL == ppObj || NULL == *ppObj) return;

	itl_SndStream it = m_SndObjStreams.begin(), itEnd = m_SndObjStreams.end();
	for(; it != itEnd; it++)
	{
		if(*ppObj == *it) 
		{
			delete *ppObj; *ppObj = NULL;
			m_SndObjStreams.erase(it);
			break;
		}
	}
}


//
//	TickTick...^^
//
void CN3SndMgr::Tick()
{
	if(!m_bSndEnable) return;

//	m_Eng.SetListenerPos(&(CN3Base::s_CameraData.vEye));
//	__Vector3 vUP(0.0f, 1.0f, 0.0f);
//	__Vector3 vDir = CN3Base::s_CameraData.vAt - CN3Base::s_CameraData.vEye;
//
//	if(vDir.Magnitude() <= FLT_MIN) vDir.Set(0.0f, 0.0f, 1.0f);
//
//	m_Eng.SetListenerOrientation(&vDir, &vUP);
//

/*
	CN3SndObj* pObj = NULL;
	itl_Snd it = m_SndObjs_Duplicated.begin(), itEnd = m_SndObjs_Duplicated.end();
	for(; it != itEnd; it++)
	{
		pObj = *it;
		pObj->Tick();
	}
*/
	itl_Snd it, itEnd;//this_Snd
	CN3SndObj* pObj = NULL;
	if(!m_bSndDuplicated)
	{
		itm_Snd it_m = m_SndObjSrcs.begin(), itEnd_m = m_SndObjSrcs.end();
		for(; it_m != itEnd_m; it_m++)
		{
			pObj = it_m->second;
			pObj->Tick();
		}
	}
	else
	{
		it = m_SndObjs_Duplicated.begin();
		itEnd = m_SndObjs_Duplicated.end();
		for(; it != itEnd; it++)
		{
			pObj = *it;
			pObj->Tick();
		}
	}


	it = m_SndObjs_PlayOnceAndRelease.begin();
	itEnd = m_SndObjs_PlayOnceAndRelease.end();
	for(; it != itEnd; )
	{
		pObj = *it;
		pObj->Tick();
		if(false == pObj->IsPlaying())
		{
			it = m_SndObjs_PlayOnceAndRelease.erase(it);
			delete pObj; pObj = NULL;
		}
		else it++;
	}

	CN3SndObjStream* pObj2 = NULL;
	itl_SndStream it2 = m_SndObjStreams.begin(), itEnd2 = m_SndObjStreams.end();
	for(; it2 != itEnd2; it2++)
	{
		pObj2 = *it2;
		if (pObj2) pObj2->Tick();
	}

//	itm_Snd it2 = m_SndObjSrcs.begin();
//	for(; it2 != m_SndObjSrcs.end(); it2++)
//	{
//		pObj = it2->second.pSndObj;
//		if(pObj) pObj->Tick();
//	}

	CN3SndObj::StaticTick(); // CommitDeferredSetting...
}


//
//	Obj하나 무효화..
void CN3SndMgr::ReleaseObj(CN3SndObj** ppObj)
{
	if(NULL == ppObj || NULL == *ppObj) return;
	std::string szFN = (*ppObj)->m_szFileName; // 파일 이름을 기억하고..

	itl_Snd it = m_SndObjs_Duplicated.begin(), itEnd = m_SndObjs_Duplicated.end();
	for(; it != itEnd; it++)
	{
		if(*ppObj == *it)
		{
			m_SndObjs_Duplicated.erase(it);
			delete *ppObj; *ppObj = NULL; // 객체 지우기..
			return;
		}
	}

	it = m_SndObjs_PlayOnceAndRelease.begin();
	itEnd = m_SndObjs_PlayOnceAndRelease.end();
	for(; it != itEnd; it++)
	{
		if(*ppObj == *it)
		{
			m_SndObjs_PlayOnceAndRelease.erase(it);
			delete *ppObj; *ppObj = NULL; // 객체 지우기..
			return;
		}
	}

	*ppObj = NULL; // 포인터만 널로 만들어 준다..

/*	itm_Snd it = m_SndObjSrcs.find(szFN);
	if(it != m_SndObjSrcs.end()) // 찾았다..
	{
		CN3SndObj* pObj = it->second;
		delete pObj;
		m_SndObjSrcs.erase(it);
	}
	else
	{
		itl_Snd it2 = m_SndObjs_PlayOnceAndRelease.begin();
		for(; it2 != m_SndObjs_PlayOnceAndRelease.end(); it2++)
		{
			CN3SndObj* pObj = *it2;
			if(pObj == *ppObj)
			{
				delete pObj;
				m_SndObjs_PlayOnceAndRelease.erase(it2);
				break;
			}		
		}
	}
*/
}


//
//	Release Whole Objects & Sound Sources & Sound Engine..
//
void CN3SndMgr::Release()
{
	if(!m_bSndEnable) return;

	CN3SndObj* pObj = NULL;
	itm_Snd it = m_SndObjSrcs.begin(), itEnd = m_SndObjSrcs.end();
	for(; it != itEnd; it++)
	{
		pObj = it->second;
		if(pObj) delete pObj;
	}
	m_SndObjSrcs.clear();

	itl_Snd it2 = m_SndObjs_Duplicated.begin(), itEnd2 = m_SndObjs_Duplicated.end();
	for(; it2 != itEnd2; it2++)
	{
		pObj = *it2;
		if(pObj) delete pObj;
	}
	m_SndObjs_Duplicated.clear();

	it2 = m_SndObjs_PlayOnceAndRelease.begin();
	itEnd2 = m_SndObjs_PlayOnceAndRelease.end();
	for(; it2 != itEnd2; it2++)
	{
		pObj = *it2;
		if(pObj) delete pObj;
	}
	m_SndObjs_PlayOnceAndRelease.clear();

	CN3SndObjStream* pObj2 = NULL;
	itl_SndStream it3 = m_SndObjStreams.begin(), itEnd3 = m_SndObjStreams.end();
	for(; it3 != itEnd3; it3++)
	{
		pObj2 = *it3;
		if(pObj2) delete pObj2;
	}
	m_SndObjStreams.clear();

	CleanupTempFiles();
	CN3SndObj::StaticRelease();
}


// 이 함수는 한번 플레이 하고 그 포인터를 다시 쓸수있게 ReleaseObj를 호출한다.
// 대신 위치는 처음 한번밖에 지정할 수 없다.
bool CN3SndMgr::PlayOnceAndRelease(int iSndID, const _D3DVECTOR* pPos)
{
	if(!m_bSndEnable) return false;

	TABLE_SOUND* pTbl = m_Tbl_Source.Find(iSndID);
	if(pTbl==NULL || pTbl->szFN.empty()) return false;
	
	CN3SndObj* pObjSrc = NULL;
	itm_Snd it = m_SndObjSrcs.find(pTbl->szFN);
	if(it == m_SndObjSrcs.end()) // 못 찾았다... 새로 만들자..
	{
		pObjSrc = new CN3SndObj();
		if(false == pObjSrc->Create(pTbl->szFN, (e_SndType)pTbl->iType)) // 새로 로딩..
		{
			delete pObjSrc; pObjSrc = NULL;
			return NULL;
		}
		m_SndObjSrcs.insert(val_Snd(pTbl->szFN, pObjSrc)); // 맵에 추가한다..
		if(!m_bSndDuplicated) pObjSrc->Play(pPos);//this_Snd
	}
	else pObjSrc = it->second;

	if(NULL == pObjSrc) return false;

	if(!m_bSndDuplicated)
	{
		pObjSrc->Play(pPos); //this_Snd
		return true;
	}

	CN3SndObj* pObj = new CN3SndObj();
	if(false == pObj->Duplicate(pObjSrc, (e_SndType)pTbl->iType)) // Duplicate 처리..
	{
		delete pObj; pObj = NULL;
		return NULL;
	}
	
	if(pObj) // 리스트에 넣는다...noah
	{
		m_SndObjs_PlayOnceAndRelease.push_back(pObj);
		pObj->Play(pPos);
		return true;
	}
	return false;
/*
	CN3SndObj* pObj = new CN3SndObj();
	if(false == pObj->Create(pTbl->szFN, (e_SndType)pTbl->iType))
	{
		delete pObj; pObj = NULL;
		return false;
	}
	pObj->Play(pPos);
	m_SndObjs_PlayOnceAndRelease.push_back(pObj);
	return true;
*/
}
