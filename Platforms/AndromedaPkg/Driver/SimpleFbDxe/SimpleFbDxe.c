/* SimpleFbDxe: Simple FrameBuffer */
#include <PiDxe.h>
#include <Uefi.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/CacheMaintenanceLib.h>
#include <Library/DebugLib.h>
#include <Library/DxeServicesTableLib.h>
#include <Library/FrameBufferBltLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/MemoryMapHelperLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>

#include <Protocol/GraphicsOutput.h>
#include <Protocol/VariableWrite.h>

/*
 * UEFIDisplayInfo
 *
 * Qualcomm's DisplayDxe normally publishes this variable so the Windows
 * Adreno KMD (qcdxkm8150.sys) can inherit the UEFI frame buffer. DisplayDxe is
 * not part of this build (SimpleFbDxe is used instead), so without this the
 * KMD still starts but finds no UEFI display buffer and the panel stays dark.
 *
 * Contract (recovered from qcdxkm8150.sys 27.20.2140.0 and DisplayDxe.efi):
 *   - Variable  : L"UEFIDisplayInfo"
 *   - GUID      : {9042a9de-23dc-4a38-96fb-7aded080516a}
 *   - Attributes: BOOTSERVICE_ACCESS | RUNTIME_ACCESS (value 6, volatile)
 *   - Size      : exactly 0x78 bytes, otherwise the KMD rejects it
 *   - byte[1]   : must be 1 (hard failure otherwise)
 *   - byte[0]   : expected 5 (mismatch only logs; gates optional fields >= 3/4)
 *   - FbSizeBytes (+0x14) must be non-zero (KMD returns failure on zero)
 *   - BufferSize  (+0x44) must be >= 2 MiB and < 4 GiB; the KMD rounds the
 *                 region to 1 MiB, so BufferBase/BufferSize are provided
 *                 1 MiB aligned to avoid it shrinking the region.
 *   - PixelFormat (+0x4c) must be 4 or 8.
 *   - DisplayIndex(+0x38): values 1..16 override the KMD's internal default
 *                 (16) of a packed config word; anything else is logged and
 *                 ignored (not fatal). Its real meaning is unknown, so it is
 *                 left at 0 here to keep the KMD's default behaviour.
 * Field names below that are marked (inferred) come from how DisplayDxe fills
 * them and how the KMD consumes them; they have not been confirmed on device.
 */
STATIC EFI_GUID mUefiDisplayInfoGuid = {
    0x9042a9de,
    0x23dc,
    0x4a38,
    {0x96, 0xfb, 0x7a, 0xde, 0xd0, 0x80, 0x51, 0x6a}};

#pragma pack(1)
typedef struct {
  UINT32 Version;       // 0x00: DisplayDxe writes 0x00AA0105 ([0]=5, [1]=1)
  UINT32 Reserved0[3];  // 0x04
  UINT32 FbAddress32;   // 0x10: (inferred) frame buffer address, low 32 bits
  UINT32 FbSizeBytes;   // 0x14: frame buffer size in bytes, must be != 0
  UINT32 BitsPerPixel;  // 0x18: (inferred)
  UINT32 Width;         // 0x1c: (inferred)
  UINT32 Height;        // 0x20: (inferred)
  UINT32 PitchBytes;    // 0x24: (inferred)
  UINT32 Reserved1;     // 0x28
  UINT32 Present;       // 0x2c: (inferred) display present flag
  UINT32 Reserved2[2];  // 0x30
  UINT32 DisplayIndex;  // 0x38: 1..16 overrides KMD default, else ignored
  UINT64 BufferBase;    // 0x3c: UEFI buffer physical base
  UINT64 BufferSize;    // 0x44: UEFI buffer size
  UINT32 PixelFormat;   // 0x4c: 4 or 8
  UINT8  Reserved3[0x78 - 0x50];
} UEFI_DISPLAY_INFO;
#pragma pack()

STATIC_ASSERT(sizeof(UEFI_DISPLAY_INFO) == 0x78, "UEFIDisplayInfo must be 0x78 bytes");

#define UEFI_DISPLAY_INFO_VERSION 0x00AA0105
#define UEFI_DISPLAY_INFO_ALIGN SIZE_1MB

STATIC EFI_RUNTIME_SERVICES *mRuntimeServices;
STATIC UEFI_DISPLAY_INFO     mUefiDisplayInfo;
STATIC BOOLEAN               mUefiDisplayInfoReady;
STATIC BOOLEAN               mUefiDisplayInfoPublished;
STATIC EFI_EVENT             mVariableWriteEvent;
STATIC VOID                 *mVariableWriteRegistration;

/*
 * Publish the variable once the variable write service is available.
 * SimpleFbDxe can be dispatched before the variable driver has finished
 * initialising (it is in APRIORI), so this is retried from a protocol notify.
 */
STATIC
EFI_STATUS
PublishUefiDisplayInfo(VOID)
{
  EFI_STATUS Status;
  VOID      *VariableWrite;

  if (mUefiDisplayInfoPublished) {
    return EFI_SUCCESS;
  }

  if (!mUefiDisplayInfoReady || mRuntimeServices == NULL) {
    return EFI_NOT_READY;
  }

  Status = gBS->LocateProtocol(
      &gEfiVariableWriteArchProtocolGuid, NULL, &VariableWrite);
  if (EFI_ERROR(Status)) {
    return EFI_NOT_READY;
  }

  Status = mRuntimeServices->SetVariable(
      L"UEFIDisplayInfo", &mUefiDisplayInfoGuid,
      EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS,
      sizeof(mUefiDisplayInfo), &mUefiDisplayInfo);

  DEBUG(
      (EFI_D_INFO, "SimpleFbDxe: UEFIDisplayInfo SetVariable: %r\n", Status));

  if (!EFI_ERROR(Status)) {
    mUefiDisplayInfoPublished = TRUE;
  }

  return Status;
}

STATIC
VOID
EFIAPI
VariableWriteNotify(IN EFI_EVENT Event, IN VOID *Context)
{
  (VOID)Context;

  if (!EFI_ERROR(PublishUefiDisplayInfo())) {
    gBS->CloseEvent(Event);
    mVariableWriteEvent = NULL;
  }
}

STATIC
VOID
SetupUefiDisplayInfo(
    IN EFI_SYSTEM_TABLE *SystemTable, IN EFI_PHYSICAL_ADDRESS FrameBufferBase,
    IN UINT32 FrameBufferSize, IN UINT64 RegionLength, IN UINT32 Width,
    IN UINT32 Height, IN UINT32 BitsPerPixel)
{
  UINT64 BufferSize;

  mRuntimeServices = SystemTable->RuntimeServices;

  /* Report the region 1 MiB aligned, never beyond what is reserved. */
  BufferSize = ALIGN_VALUE((UINT64)FrameBufferSize, UEFI_DISPLAY_INFO_ALIGN);
  if (RegionLength != 0 && BufferSize > RegionLength) {
    BufferSize = RegionLength;
  }

  ZeroMem(&mUefiDisplayInfo, sizeof(mUefiDisplayInfo));
  mUefiDisplayInfo.Version      = UEFI_DISPLAY_INFO_VERSION;
  mUefiDisplayInfo.FbAddress32  = (UINT32)FrameBufferBase;
  mUefiDisplayInfo.FbSizeBytes  = FrameBufferSize;
  mUefiDisplayInfo.BitsPerPixel = BitsPerPixel;
  mUefiDisplayInfo.Width        = Width;
  mUefiDisplayInfo.Height       = Height;
  mUefiDisplayInfo.PitchBytes   = Width * (BitsPerPixel / 8);
  mUefiDisplayInfo.Present      = 1;
  mUefiDisplayInfo.DisplayIndex = 0; /* unknown meaning: keep KMD default */
  mUefiDisplayInfo.BufferBase   = (UINT64)FrameBufferBase;
  mUefiDisplayInfo.BufferSize   = BufferSize;
  mUefiDisplayInfo.PixelFormat  = 4;
  mUefiDisplayInfoReady         = TRUE;

  if (!EFI_ERROR(PublishUefiDisplayInfo())) {
    return;
  }

  /* Variable services not up yet: publish as soon as they are. */
  if (!EFI_ERROR(gBS->CreateEvent(
          EVT_NOTIFY_SIGNAL, TPL_CALLBACK, VariableWriteNotify, NULL,
          &mVariableWriteEvent)) &&
      !EFI_ERROR(gBS->RegisterProtocolNotify(
          &gEfiVariableWriteArchProtocolGuid, mVariableWriteEvent,
          &mVariableWriteRegistration))) {
    return;
  }

  DEBUG(
      (EFI_D_ERROR,
       "SimpleFbDxe: could not arm UEFIDisplayInfo publication\n"));
}

/// Defines
/*
 * Convert enum video_log2_bpp to bytes and bits. Note we omit the outer
 * brackets to allow multiplication by fractional pixels.
 */
#define VNBYTES(bpix) (1 << (bpix)) / 8
#define VNBITS(bpix) (1 << (bpix))

#define POS_TO_FB(posX, posY)                                                  \
  ((UINT8                                                                      \
        *)((UINTN)This->Mode->FrameBufferBase + (posY)*This->Mode->Info->PixelsPerScanLine * FB_BYTES_PER_PIXEL + (posX)*FB_BYTES_PER_PIXEL))

#define FB_BITS_PER_PIXEL (32)
#define FB_BYTES_PER_PIXEL (FB_BITS_PER_PIXEL / 8)
#define DISPLAYDXE_PHYSICALADDRESS32(_x_) (UINTN)((_x_)&0xFFFFFFFF)

#define DISPLAYDXE_RED_MASK 0xFF0000
#define DISPLAYDXE_GREEN_MASK 0x00FF00
#define DISPLAYDXE_BLUE_MASK 0x0000FF
#define DISPLAYDXE_ALPHA_MASK 0x000000

/*
 * Bits per pixel selector. Each value n is such that the bits-per-pixel is
 * 2 ^ n
 */
enum video_log2_bpp {
  VIDEO_BPP1 = 0,
  VIDEO_BPP2,
  VIDEO_BPP4,
  VIDEO_BPP8,
  VIDEO_BPP16,
  VIDEO_BPP32,
};

typedef struct {
  VENDOR_DEVICE_PATH DisplayDevicePath;
  EFI_DEVICE_PATH    EndDevicePath;
} DISPLAY_DEVICE_PATH;

DISPLAY_DEVICE_PATH mDisplayDevicePath = {
    {{HARDWARE_DEVICE_PATH,
      HW_VENDOR_DP,
      {
          (UINT8)(sizeof(VENDOR_DEVICE_PATH)),
          (UINT8)((sizeof(VENDOR_DEVICE_PATH)) >> 8),
      }},
     EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID},
    {END_DEVICE_PATH_TYPE,
     END_ENTIRE_DEVICE_PATH_SUBTYPE,
     {sizeof(EFI_DEVICE_PATH_PROTOCOL), 0}}};

/// Declares

STATIC FRAME_BUFFER_CONFIGURE *mFrameBufferBltLibConfigure;
STATIC UINTN mFrameBufferBltLibConfigureSize;

STATIC
EFI_STATUS
EFIAPI
DisplayQueryMode(
    IN EFI_GRAPHICS_OUTPUT_PROTOCOL *This, IN UINT32 ModeNumber,
    OUT UINTN *SizeOfInfo, OUT EFI_GRAPHICS_OUTPUT_MODE_INFORMATION **Info);

STATIC
EFI_STATUS
EFIAPI
DisplaySetMode(IN EFI_GRAPHICS_OUTPUT_PROTOCOL *This, IN UINT32 ModeNumber);

STATIC
EFI_STATUS
EFIAPI
DisplayBlt(
    IN EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
    IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL *BltBuffer,
    OPTIONAL IN EFI_GRAPHICS_OUTPUT_BLT_OPERATION BltOperation,
    IN UINTN SourceX, IN UINTN SourceY, IN UINTN DestinationX,
    IN UINTN DestinationY, IN UINTN Width, IN UINTN Height,
    IN UINTN Delta OPTIONAL);

STATIC EFI_GRAPHICS_OUTPUT_PROTOCOL mDisplay = {
    DisplayQueryMode, DisplaySetMode, DisplayBlt, NULL};

STATIC
EFI_STATUS
EFIAPI
DisplayQueryMode(
    IN EFI_GRAPHICS_OUTPUT_PROTOCOL *This, IN UINT32 ModeNumber,
    OUT UINTN *SizeOfInfo, OUT EFI_GRAPHICS_OUTPUT_MODE_INFORMATION **Info)
{
  EFI_STATUS Status;
  Status = gBS->AllocatePool(
      EfiBootServicesData, sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION),
      (VOID **)Info);

  ASSERT_EFI_ERROR(Status);

  *SizeOfInfo                   = sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION);
  (*Info)->Version              = This->Mode->Info->Version;
  (*Info)->HorizontalResolution = This->Mode->Info->HorizontalResolution;
  (*Info)->VerticalResolution   = This->Mode->Info->VerticalResolution;
  (*Info)->PixelFormat          = This->Mode->Info->PixelFormat;
  (*Info)->PixelsPerScanLine    = This->Mode->Info->PixelsPerScanLine;

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
DisplaySetMode(IN EFI_GRAPHICS_OUTPUT_PROTOCOL *This, IN UINT32 ModeNumber)
{
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
DisplayBlt(
    IN EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
    IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL *BltBuffer,
    OPTIONAL IN EFI_GRAPHICS_OUTPUT_BLT_OPERATION BltOperation,
    IN UINTN SourceX, IN UINTN SourceY, IN UINTN DestinationX,
    IN UINTN DestinationY, IN UINTN Width, IN UINTN Height,
    IN UINTN Delta OPTIONAL)
{
  RETURN_STATUS Status;
  EFI_TPL       Tpl;
  //
  // We have to raise to TPL_NOTIFY, so we make an atomic write to the frame
  // buffer. We would not want a timer based event (Cursor, ...) to come in
  // while we are doing this operation.
  //
  Tpl    = gBS->RaiseTPL(TPL_NOTIFY);
  Status = FrameBufferBlt(
      mFrameBufferBltLibConfigure, BltBuffer, BltOperation, SourceX, SourceY,
      DestinationX, DestinationY, Width, Height, Delta);
  gBS->RestoreTPL(Tpl);

  return RETURN_ERROR(Status) ? EFI_INVALID_PARAMETER : EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
SimpleFbDxeInitialize(
    IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE *SystemTable)
{

  EFI_STATUS Status             = EFI_SUCCESS;
  EFI_HANDLE hUEFIDisplayHandle = NULL;

  /* Retrieve simple frame buffer from pre-SEC bootloader */
  DEBUG(
      (EFI_D_INFO,
       "SimpleFbDxe: Retrieve MIPI FrameBuffer parameters from PCD\n"));
  ARM_MEMORY_REGION_DESCRIPTOR_EX DisplayMemoryRegion;
  LocateMemoryMapAreaByName("Display Reserved", &DisplayMemoryRegion);
  UINT32 MipiFrameBufferAddr   = DisplayMemoryRegion.Address;
  UINT32 MipiFrameBufferWidth  = FixedPcdGet32(PcdMipiFrameBufferWidth);
  UINT32 MipiFrameBufferHeight = FixedPcdGet32(PcdMipiFrameBufferHeight);

  /* Sanity check */
  if (MipiFrameBufferAddr == 0 || MipiFrameBufferWidth == 0 ||
      MipiFrameBufferHeight == 0) {
    DEBUG((EFI_D_ERROR, "SimpleFbDxe: Invalid FrameBuffer parameters\n"));
    return EFI_DEVICE_ERROR;
  }

  /* Prepare struct */
  if (mDisplay.Mode == NULL) {
    Status = gBS->AllocatePool(
        EfiBootServicesData, sizeof(EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE),
        (VOID **)&mDisplay.Mode);

    ASSERT_EFI_ERROR(Status);
    if (EFI_ERROR(Status))
      return Status;

    ZeroMem(mDisplay.Mode, sizeof(EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE));
  }

  if (mDisplay.Mode->Info == NULL) {
    Status = gBS->AllocatePool(
        EfiBootServicesData, sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION),
        (VOID **)&mDisplay.Mode->Info);

    ASSERT_EFI_ERROR(Status);
    if (EFI_ERROR(Status))
      return Status;

    ZeroMem(mDisplay.Mode->Info, sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION));
  }

  /* Set information */
  mDisplay.Mode->MaxMode       = 1;
  mDisplay.Mode->Mode          = 0;
  mDisplay.Mode->Info->Version = 0;

  mDisplay.Mode->Info->HorizontalResolution = MipiFrameBufferWidth;
  mDisplay.Mode->Info->VerticalResolution   = MipiFrameBufferHeight;

  /* SimpleFB runs on a8r8g8b8 (VIDEO_BPP32) for WoA devices */
  UINT32               LineLength = MipiFrameBufferWidth * VNBYTES(VIDEO_BPP32);
  UINT32               FrameBufferSize    = LineLength * MipiFrameBufferHeight;
  EFI_PHYSICAL_ADDRESS FrameBufferAddress = MipiFrameBufferAddr;

  mDisplay.Mode->Info->PixelsPerScanLine = MipiFrameBufferWidth;
  mDisplay.Mode->Info->PixelFormat = PixelBlueGreenRedReserved8BitPerColor;
  mDisplay.Mode->SizeOfInfo      = sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION);
  mDisplay.Mode->FrameBufferBase = FrameBufferAddress;
  mDisplay.Mode->FrameBufferSize = FrameBufferSize;

  /* Hand the frame buffer over to the Windows GPU driver. Never fatal. */
  SetupUefiDisplayInfo(
      SystemTable, FrameBufferAddress, FrameBufferSize,
      DisplayMemoryRegion.Length, MipiFrameBufferWidth, MipiFrameBufferHeight,
      FB_BITS_PER_PIXEL);

  /* Create the FrameBufferBltLib configuration. */
  Status = FrameBufferBltConfigure(
      (VOID *)(UINTN)mDisplay.Mode->FrameBufferBase, mDisplay.Mode->Info,
      mFrameBufferBltLibConfigure, &mFrameBufferBltLibConfigureSize);

  if (Status == RETURN_BUFFER_TOO_SMALL) {
    mFrameBufferBltLibConfigure = AllocatePool(mFrameBufferBltLibConfigureSize);
    if (mFrameBufferBltLibConfigure != NULL) {
      Status = FrameBufferBltConfigure(
          (VOID *)(UINTN)mDisplay.Mode->FrameBufferBase, mDisplay.Mode->Info,
          mFrameBufferBltLibConfigure, &mFrameBufferBltLibConfigureSize);
    }
  }

  ASSERT_EFI_ERROR(Status);
  ZeroMem((VOID *)FrameBufferAddress, FrameBufferSize);

  /* Register handle */
  Status = gBS->InstallMultipleProtocolInterfaces(
      &hUEFIDisplayHandle, &gEfiDevicePathProtocolGuid, &mDisplayDevicePath,
      &gEfiGraphicsOutputProtocolGuid, &mDisplay, NULL);

  ASSERT_EFI_ERROR(Status);

  return Status;
}