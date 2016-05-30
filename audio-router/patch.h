#pragma once
#include <Audioclient.h>

// TODO: iaudioclockadjustment implementation for adjusting sample rate

template<typename T>
struct duplicate
{
    T* proxy;
    duplicate* next;

    explicit duplicate(T* proxy) : proxy(proxy), next(NULL) {}
    ~duplicate() 
    {
        if(this->proxy) 
            this->proxy->Release();
        delete this->next;
    }
    void add(T* proxy) 
    {
        duplicate** item = &this->next;
        while(*item != NULL)
            item = &(*item)->next;
        *item = new duplicate(proxy);
    }
};

typedef duplicate<IAudioClient> iaudioclient_duplicate;
typedef duplicate<IAudioRenderClient> iaudiorenderclient_duplicate;
typedef duplicate<IAudioStreamVolume> iaudiostreamvolume_duplicate;

void patch_iaudioclient(IAudioClient* host, LPGUID session_guid);
iaudioclient_duplicate* get_duplicate(IAudioClient* host);

void patch_iaudiorenderclient(IAudioRenderClient* host, WORD block_align);
iaudiorenderclient_duplicate* get_duplicate(IAudioRenderClient* host);

void patch_iaudiostreamvolume(IAudioStreamVolume* host);
iaudiostreamvolume_duplicate* get_duplicate(IAudioStreamVolume* host);