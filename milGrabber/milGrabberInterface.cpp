#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#include <windows.h>

#include <iostream>
#include <Mil.h>
#include <stdlib.h>
#include <SDKDDKVer.h>
#include <thread>

#if _DEBUG
#define DEBUG(x) x
#else
#define DEBUG(x) x
#endif

void mil_printStats(long NbProc, MIL_DOUBLE Time)
{
    DEBUG(

    MosPrintf(MIL_TEXT("%ld frames processed, at a frame rate of %.2f frames/sec ")
              MIL_TEXT("(%.2f ms/frame).\n"), NbProc, NbProc / Time, 1000.0 * Time / NbProc);
    );
}



class MilGrabber
{
public:
    MFTYPE MilGrabber() ;
    ~MilGrabber();

    MilGrabber(MilGrabber const &) = delete;
    MilGrabber(MilGrabber&&) = delete;
    MilGrabber(MilGrabber&) = delete;
    
    int MFTYPE init(char* dcfPath, int max_frames);
    int MFTYPE show() const;
    
    int MFTYPE start(int16_t grab_num_frames);
    int MFTYPE get(int16_t frame_num, uint8_t* out_buf, int32_t out_size);

    MIL_INT MFTYPE frame_size() const;
    int MFTYPE get_num_grabbed() const;

    int MFTYPE is_grabbing() const;

    int MFTYPE destroy();

    typedef enum init_err_t
    {
        IS_OK         = 0,
        ERR_NONE      = 0,
        ERR_MAPPALLOC = -1,
        ERR_MSYSALLOC = -2,
        ERR_MDIGALLOC = -3,
        ERR_MILIMAGEDISPALLOC = -4,
        ERR_MILRINGBUFFERALLOC = -5,

        ERR_TOO_MANY_FRAMES = -6,
        ERR_NOT_INITIALIZED = -7,
        ERR_ALREADY_INITIALIZED = -8,

        ERR_BUFFER_SIZE_ZERO = -9,
        ERR_BUFFER_IS_NULL = -10,
        ERR_BUFFER_SIZE_TOO_SMALL = -11,

    };

private:
    std::atomic<bool> is_initialized;
    int  max_frames;

    MIL_ID MilApplication;
    MIL_ID MilSystem;
    MIL_ID MilDigitizer;
    MIL_ID MilDisplay;
    MIL_ID MilImageDisp;

    std::vector<MIL_ID> MilImage;
    std::thread g_thread;

    std::atomic<int> num_grabbed_frames;
    std::atomic<bool> cancel;
    std::atomic<bool> grab_started;

    void grab_thread(int16_t grab_num_frames);
};


MilGrabber::MilGrabber()
{
    MilApplication = 0;
    MilSystem = 0;
    MilDigitizer = 0;
    MilDisplay = 0;
    MilImageDisp = 0;

    is_initialized = false;
    max_frames = -1;

    num_grabbed_frames = 0;
    cancel = false;
    grab_started = false;

    for (int n = 0; n < max_frames; n++)
    {
        MilImage.push_back(0);
    }

    setlocale( LC_ALL, "" );
}

MilGrabber::~MilGrabber()
{
    destroy();
}

// dcf file: "c:\\Users\\Kinga\\Desktop\\Matrox\\Testy\\rigaku.dcf"
/* Main function. */
int MFTYPE MilGrabber::init(char* dcfPath, int max_frames)
{
    /* Allocate parto of a MIL */
    MappAlloc(M_DEFAULT, &MilApplication);

    if (MilApplication == M_NULL )
    {
        return ERR_MAPPALLOC;
    }

    MsysAlloc(M_DEFAULT, MIL_TEXT("M_DEFAULT"), M_DEFAULT, M_DEFAULT, &MilSystem);
    if (MilSystem == M_NULL )
    {
        return ERR_MSYSALLOC;
    }

    // change from chart to wchar
    std::string s(dcfPath);
    std::wstring ws;
    ws.assign(s.begin(), s.end());

    MdigAlloc(MilSystem, M_DEFAULT, ws.c_str(), M_DEFAULT, &MilDigitizer);
    if (MilDigitizer == M_NULL )
    {
        return ERR_MDIGALLOC;
    }

    /* Allocate a monochrome display buffer. */
    MbufAlloc2d(MilSystem,
                MdigInquire(MilDigitizer, M_SIZE_X, M_NULL),
                MdigInquire(MilDigitizer, M_SIZE_Y, M_NULL),
                8 + M_UNSIGNED,
                M_IMAGE + M_PROC + M_DISP,
                &MilImageDisp);

    if (MilImageDisp == M_NULL )
    {
        return ERR_MILIMAGEDISPALLOC;
    }

    MbufClear(MilImageDisp, M_COLOR_BLACK);

    /* Allocate NUM_BUFFERS grab buffers. */
    for (int n = 0; n < MilImage.size(); n++)
    {
        MbufAlloc2d(MilSystem,
                    MdigInquire(MilDigitizer, M_SIZE_X, M_NULL),
                    MdigInquire(MilDigitizer, M_SIZE_Y, M_NULL),
                    8L + M_UNSIGNED,
                    M_IMAGE + M_GRAB + M_PROC, &MilImage[n]);

        if ( MilImage[n] == M_NULL)
        {
            return ERR_MILRINGBUFFERALLOC;
        }
    }

    is_initialized = true;

    return IS_OK;
}

int MFTYPE MilGrabber::show() const
{
    if (!is_initialized)
        return false;

    /* Display the image buffer. */
    MdispSelect(MilDisplay, MilImageDisp);

    return IS_OK;
}

MIL_INT MFTYPE MilGrabber::frame_size() const
{
    if (!is_initialized)
        return ERR_NOT_INITIALIZED;

    const auto x  = MdigInquire(MilDigitizer, M_SIZE_X, M_NULL);
    const auto y  = MdigInquire(MilDigitizer, M_SIZE_Y, M_NULL);
    const auto band  = MdigInquire(MilDigitizer, M_SIZE_BAND, M_NULL);

    DEBUG( MosPrintf(MIL_TEXT(" x=%i y=%i band=%i\n"), x, y, band); );

    return x*y*band;
}

int MFTYPE MilGrabber::start(int16_t grab_num_frames)
{
    if (!is_initialized)
        return ERR_NOT_INITIALIZED;

    if ( grab_num_frames >= max_frames)
        return ERR_TOO_MANY_FRAMES;

    DEBUG(MosPrintf(MIL_TEXT("start\n")); )

    MdigControl(MilDigitizer, M_GRAB_MODE, M_ASYNCHRONOUS);

    g_thread = std::thread(&MilGrabber::grab_thread, this, grab_num_frames);

    return IS_OK;
}

void MilGrabber::grab_thread(int16_t grab_num_frames)
{
    num_grabbed_frames = 0;
    grab_started = true;

    auto wait_for_first_frame = true; //states: 0- nothing, 1-wait for grabbed first frame

    try
    {
        do
        {
            DEBUG(
                int x = num_grabbed_frames;
                MosPrintf(MIL_TEXT("while: %i\n"), x); 
            )

            MdigGrab(MilDigitizer, MilImage[num_grabbed_frames]);

            //last frame is grabbed below -> #1
            if ( num_grabbed_frames < (grab_num_frames - 1))
            {
                if (wait_for_first_frame)
                {
                    wait_for_first_frame = false;
                }
                else
                {
                    num_grabbed_frames.fetch_add(1);
                }
            }

        } while ( (num_grabbed_frames < grab_num_frames) && (cancel == 0) );

        if (cancel == 0)
        {
            // Wait until the end of the last grab
            // #1
            MdigGrabWait(MilDigitizer, M_GRAB_END);
            num_grabbed_frames.fetch_add(1);
        }
    }
    catch(...)
    {
        num_grabbed_frames = 0;
    }

    grab_started = false;
}


int MFTYPE MilGrabber::get(const int16_t frame_num, uint8_t* out_buf, int32_t out_size)
{
    if (!is_initialized)
        return ERR_NOT_INITIALIZED;

    // we have one less grabbed than num_grabbed_frames, because it's asynchronous 
    if (frame_num >= (num_grabbed_frames - 1))
        return false;

    auto x = MdigInquire(MilDigitizer, M_SIZE_X, M_NULL);
    auto y = MdigInquire(MilDigitizer, M_SIZE_Y, M_NULL);
    auto band = MdigInquire(MilDigitizer, M_SIZE_BAND, M_NULL);

    if (out_size < x * y * band)
        return ERR_BUFFER_SIZE_TOO_SMALL;

    /* Copy the buffer already grabbed */
    MbufGet(MilImage[frame_num], out_buf);

    return IS_OK;
}

int MFTYPE MilGrabber::destroy()
{
    if (!is_initialized)
        return ERR_NOT_INITIALIZED;

    cancel = true;
    g_thread.join();

    /* Free allocations. */
    for (int n = 0; n < MilImage.size(); n++)
    {
        if (MilImage[n] != 0)
            MbufFree(MilImage[n]);

        MilImage[n] = 0;
    }

    /* Free display buffer. */
    if (MilImageDisp)
        MbufFree(MilImageDisp);

    MilImageDisp = 0;

    MappFreeDefault(MilApplication, MilSystem, MilDisplay, MilDigitizer, M_NULL);
    MilApplication = 0;
    MilSystem      = 0;
    MilDisplay     = 0;
    MilDigitizer   = 0;

    return IS_OK;
}

int MilGrabber::get_num_grabbed() const
{
    if (!is_initialized)
        return ERR_NOT_INITIALIZED;

    return num_grabbed_frames;
}

int MilGrabber::is_grabbing() const
{
    if (!is_initialized)
        return ERR_NOT_INITIALIZED;

    return grab_started;
}

std::unique_ptr<MilGrabber> milgrabber;

extern "C" __declspec(dllexport) int32_t MFTYPE initialize(char* dcfpath, int max_num_frames)
{
    if (milgrabber == nullptr)
    {
        milgrabber = std::make_unique<MilGrabber>();

        return milgrabber->init(dcfpath, max_num_frames);
    }

    return MilGrabber::ERR_ALREADY_INITIALIZED;
}

extern "C" __declspec(dllexport) int32_t MFTYPE start(int16_t grabFramesNo)
{
    if (milgrabber == nullptr)
        return MilGrabber::ERR_NOT_INITIALIZED;

    return milgrabber->start(grabFramesNo);
}

extern "C" __declspec(dllexport) int32_t MFTYPE destroy()
{
    if (milgrabber == nullptr)
        return MilGrabber::ERR_NOT_INITIALIZED;

    const auto result = milgrabber->destroy();
    milgrabber.reset();
    
    return result;
}

extern "C" __declspec(dllexport) int32_t MFTYPE show()
{
    if (milgrabber == nullptr)
        return MilGrabber::ERR_NOT_INITIALIZED;

    return milgrabber->show();
}

extern "C" __declspec(dllexport) MIL_INT MFTYPE get_frame_size()
{
    if (milgrabber == nullptr)
        return MilGrabber::ERR_NOT_INITIALIZED;

     return milgrabber->frame_size();
}

extern "C" __declspec(dllexport) MIL_INT MFTYPE get_num_grabbed()
{
    if (milgrabber == nullptr)
        return MilGrabber::ERR_NOT_INITIALIZED;

     return milgrabber->get_num_grabbed();
}

extern "C" __declspec(dllexport) MIL_INT MFTYPE is_grabbing()
{
    if (milgrabber == nullptr)
        return MilGrabber::ERR_NOT_INITIALIZED;

     return milgrabber->is_grabbing();
}

extern "C" __declspec(dllexport) int32_t MFTYPE get(int16_t frame_num, uint8_t* out_buf, int32_t out_size)
{
    if (milgrabber == nullptr)
        return MilGrabber::ERR_NOT_INITIALIZED;

    if (out_buf == nullptr)
        return MilGrabber::ERR_BUFFER_IS_NULL;

    if (out_buf == nullptr)
        return MilGrabber::ERR_BUFFER_SIZE_TOO_SMALL;

    return milgrabber->get(frame_num, out_buf, out_size);
}

int MosMain()
{
    return 0;
}

BOOL APIENTRY DllMain( HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
        case DLL_PROCESS_ATTACH:
        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
        case DLL_PROCESS_DETACH:
            break;
        default:;
    }
    return TRUE;
}