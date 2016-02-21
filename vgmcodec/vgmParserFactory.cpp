#include "StdAfx.h"
#include "vgmParserFactory.h"
#include <exception>


std::unique_ptr<IVgmParser> vgmParserFactory::createParserForStream(ATL::CComPtr<IMFByteStream> const& inputStream)
{
	if(inputStream == nullptr)
		throw std::invalid_argument("inputStream must be valid");
	return nullptr;
}