#include "patch.h"
#include <stdint.h>
#include <cassert>

DWORD_PTR* swap_vtable(IAudioRenderClient* this_)
{
    DWORD_PTR* old_vftptr = ((DWORD_PTR**)this_)[0];
    ((DWORD_PTR**)this_)[0] = ((DWORD_PTR***)this_)[0][5];
    return old_vftptr;
}

HRESULT __stdcall release_patch(IAudioRenderClient* this_)
{
    iaudiorenderclient_duplicate* dup = get_duplicate(this_);
    IAudioRenderClient* proxy = dup->proxy;
    UINT32* arg = ((UINT32***)this_)[0][8];
    WORD* arg2 = ((WORD***)this_)[0][9];
    DWORD_PTR* old_vftptr = swap_vtable(this_);
    ULONG result = this_->Release();
    if(result == 0)
    {
        dup->proxy = NULL;
        delete [] old_vftptr;
        delete dup;
        delete arg;
        delete arg2;
    }
    else
        ((DWORD_PTR**)this_)[0] = old_vftptr;

    return result;
}

iaudiorenderclient_duplicate* get_duplicate(IAudioRenderClient* this_)
{
    return ((iaudiorenderclient_duplicate***)this_)[0][6];
}

HRESULT __stdcall getbuffer_patch(IAudioRenderClient* this_, UINT32 NumFramesRequested, BYTE **ppData)
{
    IAudioRenderClient* proxy = get_duplicate(this_)->proxy;
    DWORD_PTR* old_vftptr = swap_vtable(this_);
    HRESULT hr = proxy->GetBuffer(NumFramesRequested, ppData);
    ((DWORD_PTR**)this_)[0] = old_vftptr;
    if(ppData != NULL && hr == S_OK && NumFramesRequested > 0)
    {
        ((BYTE***)this_)[0][7] = *ppData;
        *((UINT32***)this_)[0][8] = NumFramesRequested;
    }
    else
        ((BYTE***)this_)[0][7] = NULL;

    return hr;
}

template<typename T>
void copy_mem(BYTE* buffer2_, const BYTE* buffer_, UINT32 frames_written)
{
    const T* buffer = (const T*)buffer_;
    T* buffer2 = (T*)buffer2_;
    for(UINT32 i = 0; i < frames_written; i++)
        buffer2[i] = buffer[i];
}

HRESULT __stdcall releasebuffer_patch(IAudioRenderClient* this_, UINT32 NumFramesWritten, DWORD dwFlags)
{
    IAudioRenderClient* proxy = get_duplicate(this_)->proxy;
    const BYTE* buffer = ((BYTE***)this_)[0][7];
    const UINT32 frames_req = *((UINT32***)this_)[0][8];
    const WORD framesize = *((WORD***)this_)[0][9];
    if(buffer != NULL)
    {
        for(iaudiorenderclient_duplicate* next = get_duplicate(this_)->next; 
            next != NULL; next = next->next)
        {
            if(next->proxy)
            {
                BYTE* buffer2;
                HRESULT hr2 = next->proxy->GetBuffer(NumFramesWritten, &buffer2);
                if(hr2 == S_OK)
                {
                    DWORD flags = dwFlags;
                    switch(framesize)
                    {
                    case 1:
                        copy_mem<uint8_t>(buffer2, buffer, NumFramesWritten);
                        break;
                    case 2:
                        copy_mem<uint16_t>(buffer2, buffer, NumFramesWritten);
                        break;
                    case 4:
                        copy_mem<uint32_t>(buffer2, buffer, NumFramesWritten);
                        break;
                    case 8:
                        copy_mem<uint64_t>(buffer2, buffer, NumFramesWritten);
                        break;
                    default:
                        flags = AUDCLNT_BUFFERFLAGS_SILENT;
                        break;
                    }
                    next->proxy->ReleaseBuffer(NumFramesWritten, flags);
                }
                else if(hr2 == AUDCLNT_E_OUT_OF_ORDER)
                    next->proxy->ReleaseBuffer(0, AUDCLNT_BUFFERFLAGS_SILENT);
            }
        }
    }
    ((BYTE***)this_)[0][7] = NULL;

    DWORD_PTR* old_vftptr = swap_vtable(this_);
    HRESULT hr = proxy->ReleaseBuffer(NumFramesWritten, dwFlags);
    ((DWORD_PTR**)this_)[0] = old_vftptr;

    return hr;
}

void patch_iaudiorenderclient(IAudioRenderClient* this_, WORD block_align)
{
    // create new virtual table and save old and populate new with default
    DWORD_PTR* old_vftptr = ((DWORD_PTR**)this_)[0]; // save old virtual table
    // create new virtual table (slot 5 for old table ptr and 6 for duplicate)
    ((DWORD_PTR**)this_)[0] = new DWORD_PTR[10];
    memcpy(((DWORD_PTR**)this_)[0], old_vftptr, 5 * sizeof(DWORD_PTR));

    // created duplicate object
    iaudiorenderclient_duplicate* dup = new iaudiorenderclient_duplicate(this_);

    // patch routines
    DWORD_PTR* vftptr = ((DWORD_PTR**)this_)[0];
    vftptr[2] = (DWORD_PTR)release_patch;
    vftptr[3] = (DWORD_PTR)getbuffer_patch;
    vftptr[4] = (DWORD_PTR)releasebuffer_patch;

    vftptr[5] = (DWORD_PTR)old_vftptr;
    vftptr[6] = (DWORD_PTR)dup;
    vftptr[7] = NULL; // buffer pointer
    vftptr[8] = (DWORD_PTR)new UINT32; // NumFramesRequested
    vftptr[9] = (DWORD_PTR)new WORD; // size of audio frame

    *(WORD*)(vftptr[9]) = block_align;
}