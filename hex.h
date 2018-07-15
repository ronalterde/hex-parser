#pragma once

#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include <string.h>

namespace hex {

uint8_t hexCharToNumber(char c);
uint8_t hexCharToNumber(char high, char low);

enum class RecordType : uint8_t {
	Invalid,
	Data,
	EndOfFile
};

struct DataRecord {
	DataRecord() {
		memset(data, 0xff, RECORD_LEN_MAX);
	}
	
	static const size_t RECORD_LEN_MAX{50};
	size_t addr{0};
	uint8_t data[RECORD_LEN_MAX];
	size_t len{0};
};

class HexParser {
public:
	size_t newData(const char* inBuffer, size_t len) {
		return currentState->newData(inBuffer, len);
	}

	bool recordReady() const {
		return (currentState == &waitForStart) &&
			(recordType != RecordType::Invalid);
	}

	RecordType getRecordType() {
		return recordType;
	}

	void getData(DataRecord& dataRecord) {
		memcpy(dataRecord.data, dataBuffer, dataLen);
		dataRecord.addr = addr;
		if (recordType == RecordType::Data)
			dataRecord.len = dataLen;
		else
			dataRecord.len = 0;
	}

	int8_t getError() {
		if (currentState == &error)
			return -1;
		return 0;
	}

	void reset() {
		currentState = &waitForStart;
		recordType = RecordType::Invalid;
	}

private:
	class BaseState {
	public:
		BaseState(HexParser& context) :
			context(context) {
		}

		virtual ~BaseState() {}
		virtual size_t newData(const char*, size_t) = 0;

	protected:
		HexParser& context;
	};

	class WaitForStart : public BaseState {
	public:
		WaitForStart(HexParser& context) :
			BaseState(context) {
		}
		
		size_t newData(const char* inBuffer, size_t len) override {
			context.recordType = RecordType::Invalid;

			if (len != 1) {
				context.currentState = &context.error;
				return 1;
			}
			if (inBuffer[0] != ':') {
				context.currentState = &context.error;
				return 1;
			}

			context.currentState = &context.waitForByteCount;
			return 2;
		}
	};

	class WaitForByteCount : public BaseState {
	public:
		WaitForByteCount(HexParser& context) :
			BaseState(context) {
		}
		
		size_t newData(const char* inBuffer, size_t len) override {
			if (len != 2) {
				context.currentState = &context.error;
				return 2;
			}

			context.expectedByteCount = hexCharToNumber(inBuffer[0], inBuffer[1]);
			context.checksum += context.expectedByteCount;
			context.currentState = &context.waitForAddress;
			return 4;
		}
	};

	class WaitForAddress : public BaseState {
	public:
		WaitForAddress(HexParser& context) :
			BaseState(context) {
		}
		
		size_t newData(const char* inBuffer, size_t len) override {
			if (len != 4) {
				context.currentState = &context.error;
				return 4;
			}

			auto addrMSB = hexCharToNumber(inBuffer[0], inBuffer[1]);
			context.checksum += addrMSB;
			auto addrLSB = hexCharToNumber(inBuffer[2], inBuffer[3]);
			context.checksum += addrLSB;

			context.addr = (addrMSB << 8) | addrLSB;
			context.currentState = &context.waitForRecordType;
			return 2;
		}
	};

	class WaitForRecordType : public BaseState {
	public:
		WaitForRecordType(HexParser& context) :
			BaseState(context) {
		}
		
		size_t newData(const char* inBuffer, size_t len) override {
			if (len != 2) {
				context.currentState = &context.error;
				return 0;
			}

			if (inBuffer[0] == '0' && inBuffer[1] == '0') {
				context.checksum += 0x00;
				context.currentState = &context.waitForData;
				return context.expectedByteCount * 2;
			} else if (inBuffer[0] == '0' && inBuffer[1] == '1') {
				context.checksum += 0x01;
				context.recordType = RecordType::EndOfFile;
				context.currentState = &context.waitForChecksum;
				return 2;
			} else {
				context.currentState = &context.error;
				return 1;
			}
		}
	};

	class WaitForData : public BaseState {
	public:
		WaitForData(HexParser& context) :
			BaseState(context) {
		}
		
		size_t newData(const char* inBuffer, size_t len) override {
			if (len != context.expectedByteCount * 2) {
				context.currentState = &context.error;
				return 0;
			}

			size_t i = 0;
			while((2*i+1) < len) {
				context.dataBuffer[i] = hexCharToNumber(inBuffer[2*i], inBuffer[2*i+1]);
				context.checksum += context.dataBuffer[i];
				i++;
			}
			context.dataLen = i;

			context.recordType = RecordType::Data;
			context.currentState = &context.waitForChecksum;
			return 2;
		}
	};

	class WaitForChecksum : public BaseState {
	public:
		WaitForChecksum(HexParser& context) :
			BaseState(context) {
		}
		
		size_t newData(const char* inBuffer, size_t len) override {
			if (len != 2) {
				context.currentState = &context.error;
				return 0;
			}

			context.checksum += hexCharToNumber(inBuffer[0], inBuffer[1]);
			if ((context.checksum & 0xff) != 0) {
				context.currentState = &context.error;
				return 0;
			}
			context.currentState = &context.waitForStart;
			return 1;
		}
	};

	class Error : public BaseState {
	public:
		Error(HexParser& context) :
			BaseState(context) {
		}
		
		size_t newData(const char* inBuffer, size_t len) override {
			(void)inBuffer;
			(void)len;
			return 0;
		}
	};

private:
	static const size_t DATA_BUFFER_LEN_MAX{50};

	WaitForStart waitForStart{*this};
	WaitForByteCount waitForByteCount{*this};
	WaitForAddress waitForAddress{*this};
	WaitForRecordType waitForRecordType{*this};
	WaitForData waitForData{*this};
	WaitForChecksum waitForChecksum{*this};
	Error error{*this};
	BaseState* currentState{ &waitForStart };

	RecordType recordType{RecordType::Invalid};
	uint16_t expectedByteCount{0};
	size_t dataLen{0};
	uint8_t dataBuffer[DATA_BUFFER_LEN_MAX];
	size_t addr{0};
	uint32_t checksum{0};
};

}
