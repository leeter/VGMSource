// dllmain.h : Declaration of module class.

class CvgmcodecModule : public ATL::CAtlDllModuleT< CvgmcodecModule >
{
public :
	DECLARE_LIBID(LIBID_vgmcodecLib)
	DECLARE_REGISTRY_APPID_RESOURCEID(IDR_VGMCODEC, "{842F5473-08B4-4370-8119-CA78E8FA9F3D}")
};

extern class CvgmcodecModule _AtlModule;
