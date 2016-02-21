#pragma once
#include <memory>
#include "vgmParser.h"

class vgmParserFactory
{
public:
	static std::unique_ptr<IVgmParser> createParserForStream(ATL::CComPtr<IMFByteStream> const&);
};

