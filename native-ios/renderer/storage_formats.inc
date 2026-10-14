static const struct {const char *name;WGPUTextureFormat format;unsigned channels,bits,type;} storage_formats[]={
 {"r8unorm",WGPUTextureFormat_R8Unorm,1,8,1},{"r8snorm",WGPUTextureFormat_R8Snorm,1,8,2},{"r8uint",WGPUTextureFormat_R8Uint,1,8,3},{"r8sint",WGPUTextureFormat_R8Sint,1,8,4},
 {"rg8unorm",WGPUTextureFormat_RG8Unorm,2,8,1},{"rg8snorm",WGPUTextureFormat_RG8Snorm,2,8,2},{"rg8uint",WGPUTextureFormat_RG8Uint,2,8,3},{"rg8sint",WGPUTextureFormat_RG8Sint,2,8,4},
 {"rgba8unorm",WGPUTextureFormat_RGBA8Unorm,4,8,1},{"rgba8snorm",WGPUTextureFormat_RGBA8Snorm,4,8,2},{"rgba8uint",WGPUTextureFormat_RGBA8Uint,4,8,3},{"rgba8sint",WGPUTextureFormat_RGBA8Sint,4,8,4},{"bgra8unorm",WGPUTextureFormat_BGRA8Unorm,4,8,1},
 {"r16float",WGPUTextureFormat_R16Float,1,16,0},{"rg16float",WGPUTextureFormat_RG16Float,2,16,0},{"rgba16float",WGPUTextureFormat_RGBA16Float,4,16,0},
 {"r16uint",WGPUTextureFormat_R16Uint,1,16,3},{"rg16uint",WGPUTextureFormat_RG16Uint,2,16,3},{"rgba16uint",WGPUTextureFormat_RGBA16Uint,4,16,3},{"r16sint",WGPUTextureFormat_R16Sint,1,16,4},{"rg16sint",WGPUTextureFormat_RG16Sint,2,16,4},{"rgba16sint",WGPUTextureFormat_RGBA16Sint,4,16,4},
 {"r32float",WGPUTextureFormat_R32Float,1,32,0},{"rg32float",WGPUTextureFormat_RG32Float,2,32,0},{"rgba32float",WGPUTextureFormat_RGBA32Float,4,32,0},
 {"r32uint",WGPUTextureFormat_R32Uint,1,32,3},{"rg32uint",WGPUTextureFormat_RG32Uint,2,32,3},{"rgba32uint",WGPUTextureFormat_RGBA32Uint,4,32,3},{"r32sint",WGPUTextureFormat_R32Sint,1,32,4},{"rg32sint",WGPUTextureFormat_RG32Sint,2,32,4},{"rgba32sint",WGPUTextureFormat_RGBA32Sint,4,32,4}
};
static WGPUTextureFormat storage_format_name(const char *name){for(unsigned i=0;i<sizeof(storage_formats)/sizeof(storage_formats[0]);i++)if(!strcmp(name,storage_formats[i].name))return storage_formats[i].format;return WGPUTextureFormat_Undefined;}
static const char *storage_format_text(WGPUTextureFormat format){for(unsigned i=0;i<sizeof(storage_formats)/sizeof(storage_formats[0]);i++)if(format==storage_formats[i].format)return storage_formats[i].name;return NULL;}
static bool storage_same_type(WGPUTextureFormat a,WGPUTextureFormat b){unsigned x=9,y=10;for(unsigned i=0;i<sizeof(storage_formats)/sizeof(storage_formats[0]);i++){unsigned t=storage_formats[i].type<3?0:storage_formats[i].type;if(a==storage_formats[i].format)x=t;if(b==storage_formats[i].format)y=t;}return x==y;}
