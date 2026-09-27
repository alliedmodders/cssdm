#include <sourcemod>

#define PATCH_MAX_BYTES 64
#define PATCH_BYTE_LEN 3
#define PATCH_BYTE_SPLIT "\\x"
#define PATCH_BYTE_SPLIT_LEN 2

class MemoryPatch
{
	private bool _enabled;
	private Address _address;
	private int _offset;
	private int[] _payload;
	private int[] _restore;

	MemoryPatch(GameData gamedata, const char[] addressKey, const char[] patchKey, const char[] offsetKey)
	{
		if (gamedata == null)
		{
			SetFailState("Provided gamedata is null")
		}

		this._address = gamedata.GetMemSig(addressKey);
		if (this._address == Address_Null)
		{
			SetFailState("FFA will not work: Could not find \"%s\" signature!", addressKey);
		}

		this._offset = gamedata.GetOffset(offsetKey);
		if (this._offset == -1)
		{
			SetFailState("FFA will not work: Could not find \"%s\" offset!", offsetKey);
		}

		char bytesBuffer[PATCH_MAX_BYTES * PATCH_BYTE_SPLIT_LEN * PATCH_BYTE_LEN + 1],
			bytes[PATCH_MAX_BYTES][PATCH_BYTE_LEN];
		if (!gamedata.GetKeyValue(patchKey, bytesBuffer, sizeof(bytesBuffer)))
		{
			SetFailState("FFA will not work: Could not load \"%s\" memory patch!", patchKey);
		}

		let payloadLen = ExplodeString(bytesBuffer, PATCH_BYTE_SPLIT, bytes, PATCH_MAX_BYTES, PATCH_BYTE_LEN) - 1;

		this._payload = new int[payloadLen];
		this._restore = new int[payloadLen];

		let baseAddress = this._address + this._offset;
		for (let i = 0; i < payloadLen; i++)
		{
			this._payload[i] = StringToInt(bytes[i + 1], 16);
			this._restore[i] = LoadFromAddress(baseAddress + i, NumberType_Int8);
		}
	}

	public bool Enable()
	{
		if (this._enabled)
		{
			return false;
		}

		let baseAddress = this._address + this._offset;
		for (let i = 0; i < this._payload.size; i++)
		{
			StoreToAddress(baseAddress + i, this._payload[i], NumberType_Int8);
		}
		this._enabled = true;

		return true;
	}

	public bool Disable()
	{
		if (!this._enabled)
		{
			return false;
		}

		let baseAddress = this._address + this._offset;
		for (let i = 0; i < this._restore.size; i++)
		{
			StoreToAddress(baseAddress + i, this._restore[i], NumberType_Int8);
		}
		this._enabled = false;

		return true;
	}

	property bool Enabled
	{
		get()
		{
			return this._enabled;
		}
	}
}
