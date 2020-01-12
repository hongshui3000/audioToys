// capture.cpp
//{{{  includes defines
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <stdio.h>
#include <windows.h>

#include <mmsystem.h>
#include <mmdeviceapi.h>
#include <audioclient.h>

#include <avrt.h>
#include <functiondiscoverykeys_devpkey.h>

#include <initguid.h>

extern "C" {
  #include <libavcodec/avcodec.h>
  #include <libavcodec/avcodec.h>
  #include <libavformat/avformat.h>
  #include <libavutil/timestamp.h>
  #include <libswresample/swresample.h>
  }

#include <string>

#include "../shared/utils/cLog.h"
#include "../shared/utils/cBipBuffer.h"
//}}}
#define DEFAULT_FILE L"out.wav"

cBipBuffer bipBuffer;

#define CHANNELS 2
#define SAMPLE_RATE 48000
#define ENCODER_BITRATE 128000
//{{{
int writeData (void* file, uint8_t* data, int size) {

  fwrite (data, 1, size, (FILE*)file);
  return size;
  }
//}}}

//{{{
void sine() {

  FILE* file = fopen ("out.aac", "wb");

  AVCodec* codec = avcodec_find_encoder (AV_CODEC_ID_AAC);

  AVCodecContext* encoderContext = avcodec_alloc_context3 (codec);
  encoderContext->sample_fmt = AV_SAMPLE_FMT_FLTP;
  encoderContext->bit_rate = ENCODER_BITRATE;
  encoderContext->sample_rate = SAMPLE_RATE;
  encoderContext->channels = CHANNELS;
  encoderContext->channel_layout = av_get_default_channel_layout (CHANNELS);
  encoderContext->time_base.num = 1;
  encoderContext->time_base.den = SAMPLE_RATE;
  encoderContext->codec_type = AVMEDIA_TYPE_AUDIO ;
  avcodec_open2 (encoderContext, codec, NULL);

  // create ADTS container for encoded frames
  AVOutputFormat* outputFormat = av_guess_format ("adts", NULL, NULL);
  AVFormatContext* outputFormatContext = NULL;
  avformat_alloc_output_context2 (&outputFormatContext, outputFormat, "", NULL);

  // create ioContext for adts container with writeData callback
  int outBufferSize = 4096;
  uint8_t* outBuffer = (uint8_t*)av_malloc (outBufferSize);
  AVIOContext* ioContext = avio_alloc_context (outBuffer, outBufferSize, 1, file, NULL, &writeData, NULL);

  // link container's context to the previous I/O context
  outputFormatContext->pb = ioContext;
  AVStream* adts_stream = avformat_new_stream (outputFormatContext, NULL);
  adts_stream->id = outputFormatContext->nb_streams-1;

  // copy encoder's parameters
  avcodec_parameters_from_context (adts_stream->codecpar, encoderContext);

  // allocate stream private data and write the stream header
  avformat_write_header (outputFormatContext, NULL);

  // allocate an frame to be filled with input data.
  AVFrame* frame = av_frame_alloc();
  frame->format = AV_SAMPLE_FMT_FLTP;
  frame->channels = CHANNELS;
  frame->nb_samples = encoderContext->frame_size;
  frame->sample_rate = SAMPLE_RATE;
  frame->channel_layout = av_get_default_channel_layout (CHANNELS);

  // allocate the frame's data buffer
  av_frame_get_buffer (frame, 0);

  AVPacket* packet = av_packet_alloc();

  double t = 0.f;
  double inc = 2.0 * M_PI * 440.0 / encoderContext->sample_rate;
  for (int i = 0; i < 200; i++) {
    auto samples0 = (float*)frame->data[0];
    auto samples1 = (float*)frame->data[1];
    for (auto j = 0; j < encoderContext->frame_size; j++) {
      samples0[j] = float(sin(t));
      samples1[j] = float(sin(t));
      t += inc;
      }

    if (avcodec_send_frame (encoderContext, frame) == 0)
      while (avcodec_receive_packet (encoderContext, packet) == 0)
        if (av_write_frame (outputFormatContext, packet) < 0)
          exit(0);
    }

  // Flush cached packets
  if (avcodec_send_frame (encoderContext, NULL) == 0)
    while (avcodec_receive_packet (encoderContext, packet) == 0)
      if (av_write_frame (outputFormatContext, packet) < 0)
        exit(0);

  av_write_trailer (outputFormatContext);
  fclose (file);

  avcodec_free_context (&encoderContext);
  av_frame_free (&frame);
  avformat_free_context (outputFormatContext);
  av_freep (&ioContext);
  av_freep (&outBuffer);
  av_packet_free (&packet);

  return;
  }
//}}}

//{{{
class AudioClientStopOnExit {
public:
    AudioClientStopOnExit(IAudioClient *p) : m_p(p) {}
    ~AudioClientStopOnExit() {
        HRESULT hr = m_p->Stop();
        if (FAILED(hr)) {
            cLog::log (LOGERROR, "IAudioClient::Stop failed: hr = 0x%08x", hr);
        }
    }

private:
    IAudioClient *m_p;
};
//}}}
//{{{
class AvRevertMmThreadCharacteristicsOnExit {
public:
  AvRevertMmThreadCharacteristicsOnExit(HANDLE hTask) : m_hTask(hTask) {}

  ~AvRevertMmThreadCharacteristicsOnExit() {
    if (!AvRevertMmThreadCharacteristics(m_hTask)) {
      cLog::log (LOGERROR, "AvRevertMmThreadCharacteristics failed: last error is %d", GetLastError());
      }
    }

private:
  HANDLE m_hTask;
  };
//}}}
//{{{
class CancelWaitableTimerOnExit {
public:
  CancelWaitableTimerOnExit(HANDLE h) : m_h(h) {}

  ~CancelWaitableTimerOnExit() {
    if (!CancelWaitableTimer(m_h)) {
      cLog::log (LOGERROR, "CancelWaitableTimer failed: last error is %d", GetLastError());
      }
    }

private:
  HANDLE m_h;
  };
//}}}
//{{{
class CloseHandleOnExit {
public:
  CloseHandleOnExit(HANDLE h) : m_h(h) {}

  ~CloseHandleOnExit() {
    if (!CloseHandle(m_h)) {
      cLog::log (LOGERROR, "CloseHandle failed: last error is %d", GetLastError());
      }
    }

private:
  HANDLE m_h;
  };
//}}}
//{{{
class CoTaskMemFreeOnExit {
public:
  CoTaskMemFreeOnExit(PVOID p) : m_p(p) {}

  ~CoTaskMemFreeOnExit() {
    CoTaskMemFree(m_p);
    }

private:
  PVOID m_p;
  };
//}}}
//{{{
class CoUninitializeOnExit {
public:
  ~CoUninitializeOnExit() {
    CoUninitialize();
    }
  };
//}}}
//{{{
class PropVariantClearOnExit {
public:
    PropVariantClearOnExit(PROPVARIANT *p) : m_p(p) {}
    ~PropVariantClearOnExit() {
        HRESULT hr = PropVariantClear(m_p);
        if (FAILED(hr)) {
            cLog::log (LOGERROR, "PropVariantClear failed: hr = 0x%08x", hr);
        }
    }

private:
    PROPVARIANT *m_p;
};
//}}}
//{{{
class ReleaseOnExit {
public:
  ReleaseOnExit(IUnknown *p) : m_p(p) {}

  ~ReleaseOnExit() {
    m_p->Release();
    }

private:
  IUnknown *m_p;
  };
//}}}
//{{{
class SetEventOnExit {
public:
  SetEventOnExit(HANDLE h) : m_h(h) {}
  ~SetEventOnExit() {
    if (!SetEvent(m_h)) {
      cLog::log (LOGERROR, "SetEvent failed: last error is %d", GetLastError());
      }
    }

private:
  HANDLE m_h;
  };
//}}}
//{{{
class WaitForSingleObjectOnExit {
public:
  WaitForSingleObjectOnExit(HANDLE h) : m_h(h) {}

  ~WaitForSingleObjectOnExit() {
    DWORD dwWaitResult = WaitForSingleObject(m_h, INFINITE);
    if (WAIT_OBJECT_0 != dwWaitResult) {
      cLog::log (LOGERROR, "WaitForSingleObject returned unexpected result 0x%08x, last error is %d", dwWaitResult, GetLastError());
      }
    }

private:
  HANDLE m_h;
  };
//}}}

//{{{
HRESULT list_devices() {

  // get an enumerator
  IMMDeviceEnumerator* pMMDeviceEnumerator;
  HRESULT hr = CoCreateInstance (__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL,
                                 __uuidof(IMMDeviceEnumerator), (void**)&pMMDeviceEnumerator);
  if (FAILED (hr)) {
    //{{{
    cLog::log (LOGERROR, "CoCreateInstance(IMMDeviceEnumerator) failed: hr = 0x%08x", hr);
    return hr;
    }
    //}}}
  ReleaseOnExit releaseMMDeviceEnumerator (pMMDeviceEnumerator);

  // get all the active render endpoints
  IMMDeviceCollection* pMMDeviceCollection;
  hr = pMMDeviceEnumerator->EnumAudioEndpoints (eRender, DEVICE_STATE_ACTIVE, &pMMDeviceCollection);
  if (FAILED (hr)) {
    //{{{
    cLog::log (LOGERROR, "IMMDeviceEnumerator::EnumAudioEndpoints failed: hr = 0x%08x", hr);
    return hr;
    }
    //}}}
  ReleaseOnExit releaseMMDeviceCollection (pMMDeviceCollection);

  UINT count;
  hr = pMMDeviceCollection->GetCount (&count);
  if (FAILED(hr)) {
    //{{{
    cLog::log (LOGERROR, "IMMDeviceCollection::GetCount failed: hr = 0x%08x", hr);
    return hr;
    }
    //}}}
  cLog::log (LOGINFO, "Active render endpoints found: %u", count);

  for (UINT i = 0; i < count; i++) {
    // get the "n"th device
    IMMDevice* pMMDevice;
    hr = pMMDeviceCollection->Item (i, &pMMDevice);
    if (FAILED (hr)) {
      //{{{
      cLog::log (LOGERROR, "IMMDeviceCollection::Item failed: hr = 0x%08x", hr);
      return hr;
      }
      //}}}
    ReleaseOnExit releaseMMDevice (pMMDevice);

    // open the property store on that device
    IPropertyStore* pPropertyStore;
    hr = pMMDevice->OpenPropertyStore (STGM_READ, &pPropertyStore);
    if (FAILED(hr)) {
      //{{{
      cLog::log (LOGERROR, "IMMDevice::OpenPropertyStore failed: hr = 0x%08x", hr);
      return hr;
      }
      //}}}
    ReleaseOnExit releasePropertyStore (pPropertyStore);

    // get the long name property
    PROPVARIANT pv; PropVariantInit (&pv);
    hr = pPropertyStore->GetValue (PKEY_Device_FriendlyName, &pv);
    if (FAILED (hr)) {
      //{{{
      cLog::log (LOGERROR, "IPropertyStore::GetValue failed: hr = 0x%08x", hr);
      return hr;
      }
      //}}}

    PropVariantClearOnExit clearPv (&pv);
    if (VT_LPWSTR != pv.vt) {
      //{{{
      cLog::log (LOGERROR, "PKEY_Device_FriendlyName variant type is %u - expected VT_LPWSTR", pv.vt);
      return E_UNEXPECTED;
      }
      //}}}

    //cLog::log (LOGINFO, pv.pwszVal);
    }

  return S_OK;
  }
//}}}
//{{{
HRESULT get_default_device (IMMDevice** ppMMDevice) {

  // activate a device enumerator
  IMMDeviceEnumerator* pMMDeviceEnumerator;
  HRESULT hr = CoCreateInstance (__uuidof(MMDeviceEnumerator), NULL,
                                 CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pMMDeviceEnumerator);
  if (FAILED (hr)) {
   //{{{
   cLog::log (LOGERROR, "CoCreateInstance (IMMDeviceEnumerator) failed: hr = 0x%08x", hr);
   return hr;
    }
   //}}}
  ReleaseOnExit releaseMMDeviceEnumerator (pMMDeviceEnumerator);

  // get the default render endpoint
  hr = pMMDeviceEnumerator->GetDefaultAudioEndpoint (eRender, eConsole, ppMMDevice);
  if (FAILED (hr)) {
    //{{{
    cLog::log (LOGERROR, "IMMDeviceEnumerator::GetDefaultAudioEndpoint failed: hr = 0x%08x", hr);
    return hr;
    }
    //}}}

  return S_OK;
  }
//}}}

//{{{
HRESULT writeWaveHeader (HMMIO hFile, LPCWAVEFORMATEX pwfx, MMCKINFO* pckRIFF, MMCKINFO* pckData) {

  // make a RIFF/WAVE chunk
  pckRIFF->ckid = MAKEFOURCC ('R', 'I', 'F', 'F');
  pckRIFF->fccType = MAKEFOURCC ('W', 'A', 'V', 'E');

  MMRESULT result = mmioCreateChunk(hFile, pckRIFF, MMIO_CREATERIFF);
  if (MMSYSERR_NOERROR != result) {
    //{{{
    cLog::log (LOGERROR, "mmioCreateChunk (\"RIFF/WAVE\") failed: MMRESULT = 0x%08x", result);
    return E_FAIL;
    }
    //}}}

  // make a 'fmt ' chunk (within the RIFF/WAVE chunk)
  MMCKINFO chunk;
  chunk.ckid = MAKEFOURCC('f', 'm', 't', ' ');
  result = mmioCreateChunk(hFile, &chunk, 0);
  if (MMSYSERR_NOERROR != result) {
    //{{{
    cLog::log (LOGERROR, "mmioCreateChunk (\"fmt \") failed: MMRESULT = 0x%08x", result);
    return E_FAIL;
    }
    //}}}

  // write the WAVEFORMATEX data to it
  LONG lBytesInWfx = sizeof(WAVEFORMATEX) + pwfx->cbSize;
  LONG lBytesWritten = mmioWrite (hFile, reinterpret_cast<PCHAR>(const_cast<LPWAVEFORMATEX>(pwfx)), lBytesInWfx);
  if (lBytesWritten != lBytesInWfx) {
    //{{{
    cLog::log (LOGERROR, "mmioWrite (fmt data) wrote %u bytes; expected %u bytes", lBytesWritten, lBytesInWfx);
    return E_FAIL;
    }
    //}}}

  // ascend from the 'fmt ' chunk
  result = mmioAscend(hFile, &chunk, 0);
  if (MMSYSERR_NOERROR != result) {
    //{{{
    cLog::log (LOGERROR, "mmioAscend (\"fmt \" failed: MMRESULT = 0x%08x", result);
    return E_FAIL;
    }
    //}}}

  // make a 'fact' chunk whose data is (DWORD)0
  chunk.ckid = MAKEFOURCC('f', 'a', 'c', 't');
  result = mmioCreateChunk(hFile, &chunk, 0);
  if (MMSYSERR_NOERROR != result) {
    //{{{
    cLog::log (LOGERROR, "mmioCreateChunk (\"fmt \") failed: MMRESULT = 0x%08x", result);
    return E_FAIL;
    }
    //}}}

  // write (DWORD)0 to it
  // this is cleaned up later
  DWORD frames = 0;
  lBytesWritten = mmioWrite(hFile, reinterpret_cast<PCHAR>(&frames), sizeof(frames));
  if (lBytesWritten != sizeof(frames)) {
    //{{{
    cLog::log (LOGERROR, "mmioWrite(fact data) wrote %u bytes; expected %u bytes", lBytesWritten, (UINT32)sizeof(frames));
    return E_FAIL;
    }
    //}}}

  // ascend from the 'fact' chunk
  result = mmioAscend(hFile, &chunk, 0);
  if (MMSYSERR_NOERROR != result) {
    //{{{
    cLog::log (LOGERROR, "mmioAscend (\"fact\" failed: MMRESULT = 0x%08x", result);
    return E_FAIL;
    }
    //}}}

  // make a 'data' chunk and leave the data pointer there
  pckData->ckid = MAKEFOURCC('d', 'a', 't', 'a');
  result = mmioCreateChunk(hFile, pckData, 0);
  if (MMSYSERR_NOERROR != result) {
    //{{{
    cLog::log (LOGERROR, "mmioCreateChunk(\"data\") failed: MMRESULT = 0x%08x", result);
    return E_FAIL;
    }
    //}}}

  return S_OK;
  }
//}}}
//{{{
HRESULT finishWaveFile (HMMIO file, MMCKINFO* pckRIFF, MMCKINFO* pckData, int frames) {

  MMRESULT result;

  result = mmioAscend (file, pckData, 0);
  if (MMSYSERR_NOERROR != result) {
    cLog::log (LOGERROR, "mmioAscend(\"data\" failed: MMRESULT = 0x%08x", result);
    return E_FAIL;
    }

  result = mmioAscend (file, pckRIFF, 0);
  if (MMSYSERR_NOERROR != result) {
    cLog::log (LOGERROR, "mmioAscend(\"RIFF/WAVE\" failed: MMRESULT = 0x%08x", result);
    return E_FAIL;
    }

  result = mmioClose (file, 0);
  file = NULL;
  if (MMSYSERR_NOERROR != result) {
    //{{{
    cLog::log (LOGERROR, "mmioClose failed: MMSYSERR = %u", result);
    return -__LINE__;
    }
    //}}}

  // everything went well... fixup the fact chunk in the file

  // reopen the file in read/write mode
  MMIOINFO mi = { 0 };
  file = mmioOpen (const_cast<LPWSTR>(DEFAULT_FILE), &mi, MMIO_READWRITE);
  if (NULL == file) {
    //{{{
    cLog::log (LOGERROR, "mmioOpen failed");
    return -__LINE__;
    }
    //}}}

  // descend into the RIFF/WAVE chunk
  MMCKINFO ckRIFF = {0};
  ckRIFF.ckid = MAKEFOURCC ('W', 'A', 'V', 'E'); // this is right for mmioDescend
  result = mmioDescend (file, &ckRIFF, NULL, MMIO_FINDRIFF);
  if (MMSYSERR_NOERROR != result) {
    //{{{
    cLog::log (LOGERROR, "mmioDescend(\"WAVE\") failed: MMSYSERR = %u", result);
    return -__LINE__;
    }
    //}}}

  // descend into the fact chunk
  MMCKINFO ckFact = {0};
  ckFact.ckid = MAKEFOURCC ('f', 'a', 'c', 't');
  result = mmioDescend (file, &ckFact, &ckRIFF, MMIO_FINDCHUNK);
  if (MMSYSERR_NOERROR != result) {
    //{{{
    cLog::log (LOGERROR, "mmioDescend(\"fact\") failed: MMSYSERR = %u", result);
    return -__LINE__;
    }
    //}}}

  // write frames to the fact chunk
  LONG bytesWritten = mmioWrite (file, reinterpret_cast<PCHAR>(&frames), sizeof(frames));
  if (bytesWritten != sizeof (frames)) {
    //{{{
    cLog::log (LOGERROR, "Updating the fact chunk wrote %u bytes; expected %u", bytesWritten, (UINT32)sizeof(frames));
    return -__LINE__;
    }
    //}}}

  // ascend out of the fact chunk
  result = mmioAscend (file, &ckFact, 0);
  if (MMSYSERR_NOERROR != result) {
    //{{{
    cLog::log (LOGERROR, "mmioAscend(\"fact\") failed: MMSYSERR = %u", result);
    return -__LINE__;
    }
    //}}}

  mmioClose (file, 0);

  return S_OK;
  }
//}}}

//{{{
struct sCaptureContext {
  IMMDevice* MMDevice;
  HANDLE stopEvent;
  };
//}}}
//{{{
DWORD WINAPI captureThread (LPVOID param) {

  sCaptureContext* context = (sCaptureContext*)param;

  CoInitialize (NULL);
  CoUninitializeOnExit cuoe;

  //{{{  activate an IAudioClient
  IAudioClient* audioClient;
  if (FAILED (context->MMDevice->Activate (__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&audioClient))) {
    cLog::log (LOGERROR, "IMMDevice::Activate (IAudioClient) failed");
    return 0;
    }

  ReleaseOnExit releaseAudioClient (audioClient);
  //}}}
  //{{{  get the default device periodicity
  REFERENCE_TIME hnsDefaultDevicePeriod;
  if (FAILED (audioClient->GetDevicePeriod (&hnsDefaultDevicePeriod, NULL))) {
    cLog::log (LOGERROR, "IAudioClient::GetDevicePeriod failed");
    return 0;
    }
  //}}}
  //{{{  get the default device format
  WAVEFORMATEX* waveFormatEx;
  if (FAILED (audioClient->GetMixFormat (&waveFormatEx))) {
    cLog::log (LOGERROR, "IAudioClient::GetMixFormat failed");
    return 0;
    }

  CoTaskMemFreeOnExit freeMixFormat (waveFormatEx);
  //}}}
  //{{{  coerce int16 waveFormat, in-place not changing size of format, engine auto-convert float to int
  switch (waveFormatEx->wFormatTag) {
    case WAVE_FORMAT_IEEE_FLOAT:
      cLog::log (LOGINFO, "WAVE_FORMAT_IEEE_FLOAT");
      waveFormatEx->wFormatTag = WAVE_FORMAT_PCM;
      waveFormatEx->wBitsPerSample = 16;
      waveFormatEx->nBlockAlign = waveFormatEx->nChannels * waveFormatEx->wBitsPerSample / 8;
      waveFormatEx->nAvgBytesPerSec = waveFormatEx->nBlockAlign * waveFormatEx->nSamplesPerSec;
      break;

    case WAVE_FORMAT_EXTENSIBLE: {
      cLog::log (LOGINFO, "WAVE_FORMAT_EXTENSIBLE");
      PWAVEFORMATEXTENSIBLE pEx = reinterpret_cast<PWAVEFORMATEXTENSIBLE>(waveFormatEx);
      if (IsEqualGUID (KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, pEx->SubFormat)) {
        cLog::log (LOGINFO, "- KSDATAFORMAT_SUBTYPE_IEEE_FLOAT\n");
        pEx->SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
        pEx->Samples.wValidBitsPerSample = 16;
        waveFormatEx->wBitsPerSample = 16;
        waveFormatEx->nBlockAlign = waveFormatEx->nChannels * waveFormatEx->wBitsPerSample / 8;
        waveFormatEx->nAvgBytesPerSec = waveFormatEx->nBlockAlign * waveFormatEx->nSamplesPerSec;
        }
      else {
        cLog::log (LOGERROR, "%s", L"Don't know how to coerce mix format to int-16");
        return 0;
        }
      }
      break;

    default:
      cLog::log (LOGERROR, "Don't know how to coerce WAVEFORMATEX with wFormatTag = 0x%08x to int-16", waveFormatEx->wFormatTag);
      return 0;
    }
  //}}}

  MMIOINFO mi = { 0 };
  HMMIO file = mmioOpen (const_cast<LPWSTR>(DEFAULT_FILE), &mi, MMIO_READWRITE | MMIO_CREATE);

  MMCKINFO ckRIFF = {0};
  MMCKINFO ckData = {0};
  if (FAILED (writeWaveHeader (file, waveFormatEx, &ckRIFF, &ckData)))
    return 0;

  //{{{  create a periodic waitable timer
  HANDLE wakeUp = CreateWaitableTimer (NULL, FALSE, NULL);
  if (NULL == wakeUp) {
    cLog::log (LOGERROR, "CreateWaitableTimer failed: last error = %u", GetLastError());
    return 0;
    }

  CloseHandleOnExit closeWakeUp (wakeUp);
  //}}}
  //{{{  IAudioClient::Initialize
  // note that AUDCLNT_STREAMFLAGS_LOOPBACK and AUDCLNT_STREAMFLAGS_EVENTCALLBACK do not work together...
  // the "data ready" event never gets set so we're going to do a timer-driven loop
  if (FAILED (audioClient->Initialize (AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK, 0, 0, waveFormatEx, 0))) {
    cLog::log (LOGERROR, "IAudioClient::Initialize failed");
    return 0;
    }
  //}}}
  //{{{  activate an IAudioCaptureClient
  IAudioCaptureClient* audioCaptureClient;
  if (FAILED (audioClient->GetService (__uuidof(IAudioCaptureClient), (void**)&audioCaptureClient))) {
    cLog::log (LOGERROR, "IAudioClient::GetService (IAudioCaptureClient) failed");
    return 0;
    }

  ReleaseOnExit releaseAudioCaptureClient (audioCaptureClient);
  //}}}
  //{{{  register with MMCSS
  DWORD nTaskIndex = 0;
  HANDLE hTask = AvSetMmThreadCharacteristics (L"Audio", &nTaskIndex);
  if (NULL == hTask) {
    cLog::log (LOGERROR, "AvSetMmThreadCharacteristics failed: last error = %u", GetLastError());
    return 0;
    }

  AvRevertMmThreadCharacteristicsOnExit unregisterMmcss (hTask);
  //}}}
  //{{{  set the waitable timer
  LARGE_INTEGER liFirstFire;
  liFirstFire.QuadPart = -hnsDefaultDevicePeriod / 2; // negative means relative time
  LONG lTimeBetweenFires = (LONG)hnsDefaultDevicePeriod / 2 / (10 * 1000); // convert to milliseconds

  if (!SetWaitableTimer (wakeUp, &liFirstFire, lTimeBetweenFires, NULL, NULL, FALSE)) {
    cLog::log (LOGERROR, "SetWaitableTimer failed: last error = %u", GetLastError());
    return 0;
    }

  CancelWaitableTimerOnExit cancelWakeUp (wakeUp);
  //}}}
  //{{{  IAudioClient::Start
  if (FAILED (audioClient->Start())) {
    cLog::log (LOGERROR, "IAudioClient::Start failed");
    return 0;
    }

  AudioClientStopOnExit stopAudioClient (audioClient);
  //}}}

  // loopback capture loop
  int frames = 0;
  HANDLE waitArray[2] = { context->stopEvent, wakeUp };
  bool done = false;
  while (!done) {
    UINT32 packetSize;
    audioCaptureClient->GetNextPacketSize (&packetSize);
    while (packetSize > 0) {
      BYTE* pData;
      UINT32 numFramesToRead;
      DWORD dwFlags;
      if (FAILED (audioCaptureClient->GetBuffer (&pData, &numFramesToRead, &dwFlags, NULL, NULL))) {
        //{{{
        cLog::log (LOGERROR, "IAudioCaptureClient::GetBuffer failed");
        return 0;
        }
        //}}}
      if ((frames == 0) && AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY == dwFlags) {
        //{{{
        cLog::log (LOGINFO, "glitch on first packet");
        }
        //}}}
      else if (dwFlags != 0) {
        //{{{
        cLog::log (LOGINFO, "IAudioCaptureClient::GetBuffer flags 0x%08x", dwFlags);
        return 0;
        }
        //}}}
      if (numFramesToRead == 0) {
        //{{{
        cLog::log (LOGERROR, "IAudioCaptureClient::GetBuffer read 0 frames");
        return 0;
        }
        //}}}

      //cLog::log (LOGINFO, "numFrames %d %d %d bytes:%d\n",

      LONG bytesToWrite = numFramesToRead * waveFormatEx->nBlockAlign;
      int bytesAllocated = 0;
      uint8_t* ptr = bipBuffer.reserve (bytesToWrite, bytesAllocated);
      if (ptr && (bytesAllocated == bytesToWrite)) {
        memcpy (ptr, pData, bytesAllocated);
        bipBuffer.commit (bytesAllocated);
        }

      LONG bytesWritten = mmioWrite (file, reinterpret_cast<PCHAR>(pData), bytesToWrite);
      if (bytesWritten != bytesToWrite) {
        //{{{
        cLog::log (LOGERROR, "mmioWrite wrote %u bytes expected %u bytes", bytesWritten, bytesToWrite);
        return 0;
        }
        //}}}
      if (FAILED (audioCaptureClient->ReleaseBuffer (numFramesToRead))) {
        //{{{
        cLog::log (LOGERROR, "IAudioCaptureClient::ReleaseBuffer failed");
        return 0;
        }
        //}}}

      frames += numFramesToRead;
      audioCaptureClient->GetNextPacketSize (&packetSize);
      }

    DWORD dwWaitResult = WaitForMultipleObjects (ARRAYSIZE(waitArray), waitArray, FALSE, INFINITE);
    if (WAIT_OBJECT_0 == dwWaitResult) {
      //{{{
      cLog::log (LOGINFO, "Received stop event");
      done = true;
      }
      //}}}
    else if (WAIT_OBJECT_0 + 1 != dwWaitResult) {
      //{{{
      cLog::log (LOGERROR, "Unexpected WaitForMultipleObjects return value %u", dwWaitResult);
      return 0;
      }
      //}}}
    }

  finishWaveFile (file, &ckData, &ckRIFF, frames);

  return 0;
  }
//}}}
//{{{
DWORD WINAPI readThread (LPVOID context) {

  sCaptureContext* captureContext = (sCaptureContext*)context;

  CoInitialize (NULL);
  CoUninitializeOnExit cuoe;

  FILE* file = fopen ("out.aac", "wb");

  AVCodec* codec = avcodec_find_encoder (AV_CODEC_ID_AAC);

  AVCodecContext* encoderContext = avcodec_alloc_context3 (codec);
  encoderContext->sample_fmt = AV_SAMPLE_FMT_FLTP;
  encoderContext->bit_rate = ENCODER_BITRATE;
  encoderContext->sample_rate = SAMPLE_RATE;
  encoderContext->channels = CHANNELS;
  encoderContext->channel_layout = av_get_default_channel_layout (CHANNELS);
  encoderContext->time_base.num = 1;
  encoderContext->time_base.den = SAMPLE_RATE;
  encoderContext->codec_type = AVMEDIA_TYPE_AUDIO ;
  avcodec_open2 (encoderContext, codec, NULL);

  // create ADTS container for encoded frames
  AVOutputFormat* outputFormat = av_guess_format ("adts", NULL, NULL);
  AVFormatContext* outputFormatContext = NULL;
  avformat_alloc_output_context2 (&outputFormatContext, outputFormat, "", NULL);

  // create ioContext for adts container with writeData callback
  int outBufferSize = 4096;
  uint8_t* outBuffer = (uint8_t*)av_malloc (outBufferSize);
  AVIOContext* ioContext = avio_alloc_context (outBuffer, outBufferSize, 1, file, NULL, &writeData, NULL);

  // link container's context to the previous I/O context
  outputFormatContext->pb = ioContext;
  AVStream* adts_stream = avformat_new_stream (outputFormatContext, NULL);
  adts_stream->id = outputFormatContext->nb_streams-1;

  // copy encoder's parameters
  avcodec_parameters_from_context (adts_stream->codecpar, encoderContext);

  // allocate stream private data and write the stream header
  avformat_write_header (outputFormatContext, NULL);

  // allocate an frame to be filled with input data.
  AVFrame* frame = av_frame_alloc();
  frame->format = AV_SAMPLE_FMT_FLTP;
  frame->channels = CHANNELS;
  frame->nb_samples = encoderContext->frame_size;
  frame->sample_rate = SAMPLE_RATE;
  frame->channel_layout = av_get_default_channel_layout (CHANNELS);

  // allocate the frame's data buffer
  av_frame_get_buffer (frame, 0);

  AVPacket* packet = av_packet_alloc();

  while (true) {
    int len = encoderContext->frame_size;
    auto ptr = (int16_t*)bipBuffer.getContiguousBlock (len);
    if (len >= encoderContext->frame_size) {
      cLog::log (LOGINFO, "read block %d", len);
      auto samples0 = (float*)frame->data[0];
      auto samples1 = (float*)frame->data[1];
      for (auto j = 0; j < encoderContext->frame_size; j++) {
        samples0[j] = float(*ptr++) / 0x10000;
        samples1[j] = float(*ptr++) / 0x10000;
        }
      bipBuffer.decommitBlock (len);

      if (avcodec_send_frame (encoderContext, frame) == 0)
        while (avcodec_receive_packet (encoderContext, packet) == 0)
          if (av_write_frame (outputFormatContext, packet) < 0)
            exit(0);
      }
    else
      Sleep (1);
    }

  // Flush cached packets
  if (avcodec_send_frame (encoderContext, NULL) == 0)
    while (avcodec_receive_packet (encoderContext, packet) == 0)
      if (av_write_frame (outputFormatContext, packet) < 0)
        exit(0);

  av_write_trailer (outputFormatContext);
  fclose (file);

  avcodec_free_context (&encoderContext);
  av_frame_free (&frame);
  avformat_free_context (outputFormatContext);
  av_freep (&ioContext);
  av_freep (&outBuffer);
  av_packet_free (&packet);

  return 0;
  }
//}}}

//{{{
void avLogCallback (void* ptr, int level, const char* fmt, va_list vargs) {
  cLog::log (LOGINFO, fmt, vargs);
  }
//}}}
//{{{
int main() {

  cLog::init (LOGINFO, false, "");
  cLog::log (LOGNOTICE, "capture");

  //av_log_set_level (AV_LOG_VERBOSE);
  av_log_set_callback (avLogCallback);

  CoInitialize (NULL);
  CoUninitializeOnExit cuoe;

  bipBuffer.allocateBuffer (1024 * 32);

  sCaptureContext captureContext;

  list_devices();
  get_default_device (&captureContext.MMDevice);

  // create a "stop capturing now" event
  captureContext.stopEvent = CreateEvent (NULL, FALSE, FALSE, NULL);
  if (NULL == captureContext.stopEvent) {
    //{{{
    cLog::log (LOGERROR, "CreateEvent failed: last error is %u", GetLastError());
    return -__LINE__;
    }
    //}}}
  CloseHandleOnExit closeStopEvent (captureContext.stopEvent);

  HANDLE hReadThread = CreateThread (NULL, 0, readThread, &captureContext, 0, NULL );
  if (hReadThread == NULL) {
    //{{{
    cLog::log (LOGERROR, "CreateThread failed: last error is %u", GetLastError());
    return -__LINE__;
    }
    //}}}
  CloseHandleOnExit closeThread1 (hReadThread);

  HANDLE hThread = CreateThread (NULL, 0, captureThread, &captureContext, 0, NULL );
  if (hThread == NULL) {
    //{{{
    cLog::log (LOGERROR, "CreateThread failed: last error is %u", GetLastError());
    return -__LINE__;
    }
    //}}}
  CloseHandleOnExit closeThread (hThread);

  // at this point capture is running .wait for the user to press a key or for capture to error out
  WaitForSingleObjectOnExit waitForThread (hThread);
  SetEventOnExit setStopEvent (captureContext.stopEvent);
  HANDLE hStdIn = GetStdHandle (STD_INPUT_HANDLE);
  if (INVALID_HANDLE_VALUE == hStdIn) {
    //{{{
    cLog::log (LOGERROR, "GetStdHandle returned INVALID_HANDLE_VALUE: last error is %u", GetLastError());
    return -__LINE__;
    }
    //}}}

  HANDLE rhHandles[2] = { hThread, hStdIn };
  bool bKeepWaiting = true;
  while (bKeepWaiting) {
    auto dwWaitResult = WaitForMultipleObjects (2, rhHandles, FALSE, INFINITE);
    switch (dwWaitResult) {
      case WAIT_OBJECT_0: // hThread
        cLog::log (LOGERROR, "%s", L"The thread terminated early - something bad happened");
        bKeepWaiting = false;
        break;

      case WAIT_OBJECT_0 + 1: // hStdIn
        // see if any of them was an Enter key-up event
        INPUT_RECORD rInput[128];
        DWORD nEvents;
        if (!ReadConsoleInput (hStdIn, rInput, ARRAYSIZE(rInput), &nEvents)) {
          //{{{
          cLog::log (LOGERROR, "ReadConsoleInput failed: last error is %u", GetLastError());
          bKeepWaiting = false;
          }
          //}}}
        else {
          for (DWORD i = 0; i < nEvents; i++) {
            if (KEY_EVENT == rInput[i].EventType &&
                VK_RETURN == rInput[i].Event.KeyEvent.wVirtualKeyCode && !rInput[i].Event.KeyEvent.bKeyDown) {
              //{{{
              cLog::log (LOGINFO, "Stopping capture...");
              bKeepWaiting = false;
              break;
              }
              //}}}
              }
            // if none of them were Enter key-up events continue waiting
            }
          break;

      default:
        cLog::log (LOGERROR, "WaitForMultipleObjects returned unexpected value 0x%08x", dwWaitResult);
        bKeepWaiting = false;
        break;
      }
    }

  if (NULL != captureContext.MMDevice)
    captureContext.MMDevice->Release();

  // let prefs' destructor call mmioClose
  return 0;
  }
//}}}
