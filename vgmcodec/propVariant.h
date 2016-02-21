#pragma once
class propVariant : public tagPROPVARIANT
{
	inline void Clear() 
	{
		_com_util::CheckError(::PropVariantClear(this));
	}
	
public:
	inline propVariant(){ ::PropVariantInit(this); };
	propVariant(propVariant const&) = default;
	inline ~propVariant(void){ ::PropVariantClear(this); };

	propVariant& operator=(propVariant const&) = default;

	inline propVariant& operator=(__int64 i8Src) 
	{
		//_variant_t
		if (this->vt != VT_I8) {
			Clear();
			this->vt = VT_I8;
		}

		this->hVal.QuadPart = i8Src;

		return *this;
	}

	inline propVariant(IUnknown* pSrc, bool fAddRef = true) _NOEXCEPT
	{
		V_VT(this) = VT_UNKNOWN;
		V_UNKNOWN(this) = pSrc;

		// Need the AddRef() as VariantClear() calls Release(), unless fAddRef
		// false indicates we're taking ownership
		//
		if (fAddRef) {
			if (V_UNKNOWN(this) != nullptr) {
				V_UNKNOWN(this)->AddRef();
			}
		}
	}

	inline propVariant& operator=(IUnknown* pSrc) 
	{
		_COM_ASSERT(V_VT(this) != VT_UNKNOWN || pSrc == nullptr || V_UNKNOWN(this) != pSrc);

		// Clear VARIANT (This will Release() any previous occupant)
		//
		Clear();

		V_VT(this) = VT_UNKNOWN;
		V_UNKNOWN(this) = pSrc;

		if (V_UNKNOWN(this) != nullptr) {
			// Need the AddRef() as VariantClear() calls Release()
			//
			V_UNKNOWN(this)->AddRef();
		}

		return *this;
	}
};