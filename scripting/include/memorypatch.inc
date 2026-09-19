// based on https://github.com/Kenzzer/MemoryPatch/blob/master/addons/sourcemod/scripting/include/memorypatch.inc

#define PATCH_MAX_BYTES 64
#define PATCH_BYTE_LEN 3
#define PATCH_BYTE_SPLIT "\\x"
#define PATCH_BYTE_SPLIT_LEN 2

enum
{
	MemoryIndex_Address = 0,
	MemoryIndex_PayloadCount,
	MemoryIndex_Patched,
	MemoryIndex_Max
};

methodmap MemoryPatch < ArrayList
{
	public MemoryPatch(GameData hGameData = null, const char[] addressKey, const char[] patchKey, const char[] offsetKey)
	{
		if (hGameData == null)
		{
			LogError("Attempted to create a MemoryPatch without gamedata.");
			return null;
		}
		
		ArrayList patch = new ArrayList();
		
		Address address = hGameData.GetMemSig(addressKey);
		if (address == Address_Null)
		{
			delete patch;
			LogError("FFA will not work: Could not find \"%s\" signature!", addressKey);
			return null;
		}

		int offset = hGameData.GetOffset(offsetKey);
		if (offset == -1)
		{
			delete patch;
			LogError("FFA will not work: Could not find \"%s\" offset!", offsetKey);
			return null;
		}
		Address finalAddress = address + view_as<Address>(offset);
		
		char sBytesBuffer[PATCH_MAX_BYTES*PATCH_BYTE_SPLIT_LEN*PATCH_BYTE_LEN+1], sBytes[PATCH_MAX_BYTES][PATCH_BYTE_LEN];
		if (!hGameData.GetKeyValue(patchKey, sBytesBuffer, sizeof(sBytesBuffer)))
		{
			delete patch;
			LogError("FFA will not work: Could not load \"%s\" memory patch!", patchKey);
			return null;
		}
		
		int iPayloadCount = ExplodeString(sBytesBuffer, PATCH_BYTE_SPLIT, sBytes, PATCH_MAX_BYTES, PATCH_BYTE_LEN);
		
		patch.Push(finalAddress); // MemoryIndex_Address
		patch.Push(iPayloadCount-1); // MemoryIndex_PayloadCount
		patch.Push(false); // MemoryIndex_Patched
		for (int i = 1; i < iPayloadCount; i++)
		{
			patch.Push(StringToInt(sBytes[i], 16)); // Push the bytes
		}

		return view_as<MemoryPatch>(patch);
	}
	
	property bool bEnabled
	{
		public get()
		{
			return this.Get(MemoryIndex_Patched);
		}
	}
	
	public void Toggle()
	{
		Address address = this.Get(MemoryIndex_Address);
#if defined DEBUG
		LogMessage("Patching address 0x%X...", address);
#endif
		
		for (int i = 0, iMax = this.Get(MemoryIndex_PayloadCount); i < iMax; i++)
		{
			Address byte = this.Get(i+MemoryIndex_Max);
			Address originalByte = view_as<Address>(LoadFromAddress(address+view_as<Address>(i), NumberType_Int8));
			
#if defined DEBUG
			LogMessage("[%i] [%02X] --> [%02X]", i, originalByte, byte);
#endif
			this.Set(i+MemoryIndex_Max, originalByte);
			StoreToAddress(address+view_as<Address>(i), view_as<int>(byte), NumberType_Int8);
		}
	}
	
	public void Enable()
	{
		if (this.bEnabled) return;
		
		this.Toggle();
		this.Set(MemoryIndex_Patched, true);
	}
	
	public void Disable()
	{
		if (!this.bEnabled) return;
		
		this.Toggle();
		this.Set(MemoryIndex_Patched, false);
	}
}
