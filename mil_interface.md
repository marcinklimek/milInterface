# MIL Interface for LabView
Marcin Klimek
2021


Interface
---------

#### initialize
```
int32_t initialize(char* dcfpath, int max_num_frames) 
```
returns Error code, max_num_frames -> maximum number of frames possible to grab in one start session 

#### destroy
```
int32_t destroy()
```
returns Error code

#### get_frame_size
```
MIL_INT get_frame_size() 
```
returns size or Error code

#### start
```
int32_t start(int16_t grab_n_frames) 
```
returns Error code
condition: grab_n_frames < max_num_frames

#### get_num_grabbed
```
MIL_INT get_num_grabbed()
```
returns number of already grabbed frames or Error code

#### get
```
int32_t get(int16_t frame_idx, uint8_t* outBuf, int32_t outSize) 
```
returns Error code + image in outBuf
condition: (frame_idx < (num_grabbed_frames - 1))

#### is_grabbing
```
MIL_INT is_grabbing() -> returns 1 if is working, 0 if not otherwise Error code
```  

Usage
-----

1) initialize
2) start( number of frames)
3) work, check in loop:
```
    if is_grabbing()
    {
      get_num_grabbed()
      if any new frame:
         get(frame_num) // frame_num -> index of the grabbed frame, less than num_grabbed_frames
    }
    else
      //done
```      
4) next step
```
    if needed
       goto #2 (start)
    else
       goto #5
```
5) destroy

Error codes
-----------

- IS_OK         = 0,
- ERR_MAPPALLOC = -1,
- ERR_MSYSALLOC = -2,
- ERR_MDIGALLOC = -3,
- ERR_MILIMAGEDISPALLOC = -4,
- ERR_MILRINGBUFFERALLOC = -5,
- ERR_TOO_MANY_FRAMES = -6,
- ERR_NOT_INITIALIZED = -7,
- ERR_ALREADY_INITIALIZED = -8,
- ERR_BUFFER_SIZE_ZERO = -9,
- ERR_BUFFER_IS_NULL = -10,
- ERR_BUFFER_SIZE_TOO_SMALL = -11,

Additional types
------
```
typedef  long long  MIL_INT;  -> int64_t
```