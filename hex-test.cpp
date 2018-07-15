#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "hex.h"
#include <cstring>

using namespace std;
using namespace hex;

namespace {

void makeChecksum(const char* record, char* checksumString) {
	uint32_t checksum = 0;
	const char* p = record;
	while (*p != '\0') {
		checksum += hexCharToNumber(p[0], p[1]);
		p += 2;
	}
	checksum = ~(checksum & 0xff);
	checksum = checksum + 1;

	checksum = checksum & 0xff;
	sprintf(checksumString, "%02x", (uint8_t)checksum);
}

class HexParserTest : public ::testing::Test {
protected:
	void SetUp() override {
		memset(inputBuffer, 0x00, 500);
	}

	void feedInCompleteDataRecord(const char* byteCount,
			const char* addr, const char* data, const char* checksum) {
		feedInStartChar();
		feedInByteCount(byteCount);
		feedInAddr(addr);
		feedInRecordType("00");
		feedIn(data);
		feedIn(checksum);
	}

	void feedInCompleteDataRecord() {
		feedInCompleteDataRecord("03", "1AbF", "0a0b0c", "03");
	}

	void feedInCompleteDataRecordWithAddress(const char* addr) {
		char str[100];
		sprintf(str, "05%s000a0b0c00", addr);
		char checksumString[3] = {0};
		makeChecksum(str, checksumString);

		feedInCompleteDataRecord("05", addr, "000a0b0c00", checksumString);
	}

	void feedInCompleteEofRecord() {
		feedInStartChar();
		feedInByteCount("00");
		feedInAddr();
		feedInRecordType("01");
		feedIn("ff");
	}

	void feedInStartChar() {
		feedIn(":");
	}

	void feedInByteCount(const char* str) {
		feedIn(str);
	}

	void feedInByteCount() {
		feedInByteCount("10");
	}

	void feedInAddr(const char* str) {
		feedIn(str);
	}

	void feedInAddr() {
		feedIn("1AbF");
	}

	void feedInRecordType(const char* str) {
		feedIn(str);
	}

	void feedIn(const char* str) {
		auto len = strlen(str);
		parser.newData(str, len);
	}

protected:
	HexParser parser;
	char inputBuffer[500];
	DataRecord dataRecord;
};

TEST_F(HexParserTest, acceptsStartChar) {
	inputBuffer[0] = ':';
	parser.newData(inputBuffer, 1);
	ASSERT_EQ(parser.getError(), 0);
}

TEST_F(HexParserTest, doesNotAcceptAnyOtherStartChar) {
	inputBuffer[0] = 'a';
	parser.newData(inputBuffer, 1);
	ASSERT_NE(parser.getError(), 0);
}

TEST_F(HexParserTest, errorCanBeReset) {
	inputBuffer[0] = 'a';
	parser.newData(inputBuffer, 1);
	ASSERT_NE(parser.getError(), 0);
	parser.reset();
	ASSERT_EQ(parser.getError(), 0);
}

TEST_F(HexParserTest, doesNotAcceptStartCharWithWrongLen) {
	inputBuffer[0] = ':';
	parser.newData(inputBuffer, 2);
	ASSERT_NE(parser.getError(), 0);
}

TEST_F(HexParserTest, requestsTwoByteCountCharsAfterStart) {
	inputBuffer[0] = ':';
	ASSERT_EQ(parser.newData(inputBuffer, 1), 2);
}

class HexParserTestWithMultipleByteCounts : public HexParserTest,
                public ::testing::WithParamInterface<const char*> {
};

INSTANTIATE_TEST_CASE_P(HexParserTestWithMultipleByteCounts, HexParserTestWithMultipleByteCounts,
		::testing::Values(
			"0",
			"000",
			"0000",
			"00000"
		)
);

TEST_P(HexParserTestWithMultipleByteCounts, doesNotAcceptByteCountWithWrongLen) {
	feedInStartChar();
	feedIn(GetParam());
	ASSERT_NE(parser.getError(), 0);
}

TEST_F(HexParserTest, requestsFourAddressCharsAfterByteCount) {
	feedInStartChar();
	inputBuffer[0] = '1';
	inputBuffer[1] = '0';
	ASSERT_EQ(parser.newData(inputBuffer, 2), 4);
}

TEST_F(HexParserTest, requestsTwoRecordTypeCharsAfterAddress) {
	feedInStartChar();
	feedInByteCount();
	ASSERT_EQ(parser.newData(inputBuffer, 4), 2);
}

TEST_F(HexParserTest, doesNotAcceptAddressWithWrongLen) {
	feedInStartChar();
	feedInByteCount();
	feedInAddr("0");
	ASSERT_NE(parser.getError(), 0);
}

TEST_F(HexParserTest, doesNotAcceptUnsupportedRecordTypes) {
	feedInStartChar();
	feedInByteCount();
	feedInAddr();
	feedInRecordType("03");
	ASSERT_NE(parser.getError(), 0);
}

TEST_F(HexParserTest, doesNotAcceptRecordTypeWithWrongLen) {
	feedInStartChar();
	feedInByteCount();
	feedInAddr();
	feedInRecordType("000");
	ASSERT_NE(parser.getError(), 0);
}

struct ByteCountTestParam {
	char byteCountInput[50];
	uint16_t expectedCharCount;
};

class HexParserTestWithParam : public HexParserTest,
                public ::testing::WithParamInterface<ByteCountTestParam> {
};

INSTANTIATE_TEST_CASE_P(HexParserTestWithParam, HexParserTestWithParam,
		::testing::Values(
			ByteCountTestParam{"00", 0},
			ByteCountTestParam{"03", 6},
			ByteCountTestParam{"05", 10}
		)
);

TEST_P(HexParserTestWithParam, requestsDataBytesAccordingToByteCount) {
	feedInStartChar();
	feedInByteCount(GetParam().byteCountInput);
	feedInAddr();

	inputBuffer[0] = '0';
	inputBuffer[1] = '0';
	ASSERT_EQ(parser.newData(inputBuffer, 2), GetParam().expectedCharCount);
}

TEST_F(HexParserTest, doesNotAcceptDataWithWrongLength) {
	feedInStartChar();
	feedInByteCount("ff");
	feedInAddr();
	feedInRecordType("00");
	feedIn("abcdefg123456");
	ASSERT_NE(parser.getError(), 0);
}

TEST_F(HexParserTest, requestsChecksumAfterData) {
	feedInStartChar();
	feedInByteCount("03");
	feedInAddr();
	feedInRecordType("00");

	ASSERT_EQ(parser.newData(inputBuffer, 6), 2);
}

TEST_F(HexParserTest, doesNotRequestAnyDataForEofRecord) {
	feedInStartChar();
	feedInByteCount("00");
	feedInAddr();

	inputBuffer[0] = '0';
	inputBuffer[1] = '1';
	ASSERT_EQ(parser.newData(inputBuffer, 2), 2);
}

TEST_F(HexParserTest, expectsStartOfAnotherLineAfterChecksum) {
	feedInStartChar();
	feedInByteCount("01");
	feedInAddr("0100");
	feedInRecordType("00");
	feedIn("01");

	inputBuffer[0] = 'f';
	inputBuffer[1] = 'd';
	ASSERT_EQ(parser.newData(inputBuffer, 2), 1);
}

TEST_F(HexParserTest, doesNotAcceptWrongChecksum) {
	feedInCompleteDataRecord("03", "1AbF", "0a0b0c", "ff");
	ASSERT_NE(parser.getError(), 0);
}

TEST_F(HexParserTest, doesNotAcceptChecksumWithWrongLen) {
	feedInStartChar();
	feedInByteCount("01");
	feedInAddr("0000");
	feedInRecordType("00");
	feedIn("00");
	feedIn("ff000");
	ASSERT_NE(parser.getError(), 0);
}

TEST_F(HexParserTest, readyFlagIsSetAfterChecksum) {
	ASSERT_FALSE(parser.recordReady());
	feedInStartChar();
	ASSERT_FALSE(parser.recordReady());
	feedInByteCount("01");
	ASSERT_FALSE(parser.recordReady());
	feedInAddr("0101");
	ASSERT_FALSE(parser.recordReady());
	feedInRecordType("00");
	ASSERT_FALSE(parser.recordReady());
	feedIn("02");
	ASSERT_FALSE(parser.recordReady());
	feedIn("fb");
	ASSERT_TRUE(parser.recordReady());
}

TEST_F(HexParserTest, readyFlagIsClearedOnReceptionOfTheNextStart) {
	feedInCompleteDataRecord();
	feedInStartChar();
	ASSERT_FALSE(parser.recordReady());
}

TEST_F(HexParserTest, readyFlagIsClearedOnReset) {
	feedInCompleteDataRecord();
	parser.reset();
	ASSERT_FALSE(parser.recordReady());
}

TEST_F(HexParserTest, recordTypeGetsValidAfterParse) {
	ASSERT_EQ(parser.getRecordType(), RecordType::Invalid);
	feedInCompleteDataRecord();
	ASSERT_NE(parser.getRecordType(), RecordType::Invalid);
}

TEST_F(HexParserTest, recordTypeIsClearedOnReceptionOfTheNextStart) {
	feedInCompleteDataRecord();
	feedInStartChar();
	ASSERT_EQ(parser.getRecordType(), RecordType::Invalid);
}

TEST_F(HexParserTest, eofRecordType) {
	feedInCompleteEofRecord();
	ASSERT_EQ(parser.getRecordType(), RecordType::EndOfFile);
}

TEST_F(HexParserTest, dataRecordType) {
	feedInCompleteDataRecord();
	ASSERT_EQ(parser.getRecordType(), RecordType::Data);
}

TEST_F(HexParserTest, getDataReturnsZeroLenAfterInit) {
	DataRecord dataRecord;
	parser.getData(dataRecord);
	ASSERT_EQ(dataRecord.len, 0);
}

TEST_F(HexParserTest, getDataReturnsZeroAddrAfterInit) {
	DataRecord dataRecord;
	parser.getData(dataRecord);
	ASSERT_EQ(dataRecord.addr, 0x0000);
}

TEST_F(HexParserTest, getDataYieldsAddress) {
	feedInCompleteDataRecordWithAddress("1AbF");
	parser.getData(dataRecord);
	ASSERT_EQ(dataRecord.addr, 0x1abf);

	feedInCompleteDataRecordWithAddress("beEF");
	parser.getData(dataRecord);
	ASSERT_EQ(dataRecord.addr, 0xbeef);

	feedInCompleteDataRecordWithAddress("FFFF");
	parser.getData(dataRecord);
	ASSERT_EQ(dataRecord.addr, 0xffff);

	feedInCompleteDataRecordWithAddress("0000");
	parser.getData(dataRecord);
	ASSERT_EQ(dataRecord.addr, 0x0000);
}

TEST_F(HexParserTest, getDataForCompleteDataRecord) {
	feedInCompleteDataRecord("05", "1AbF", "000a0b0c00", "da");

	parser.getData(dataRecord);
	ASSERT_EQ(dataRecord.len, 5);

	DataRecord dataRecordExpected;
	uint8_t expected[] = { 0x00, 0x0a, 0x0b, 0x0c, 0x00 };
	memcpy(dataRecordExpected.data, expected, 5);	
	dataRecordExpected.addr = 0x1abf;
	ASSERT_THAT(dataRecord.data, ::testing::ContainerEq(dataRecordExpected.data));
	ASSERT_EQ(dataRecord.addr, dataRecordExpected.addr);
}

TEST_F(HexParserTest, integration) {
	const char* hexLine = ":10010000214601360121470136007EFE09D2190140";
	const char* p = hexLine;

	size_t requestedCharCountPrev = 1;
	size_t requestedCharCount = 1;
	while(!parser.recordReady()) {
		ASSERT_EQ(parser.getError(), 0);
		requestedCharCountPrev = requestedCharCount;
		requestedCharCount = parser.newData(p, requestedCharCountPrev);
		p += requestedCharCountPrev;
	}
	ASSERT_EQ(parser.getRecordType(), RecordType::Data);

	DataRecord dataRecord;
	parser.getData(dataRecord);
	ASSERT_EQ(dataRecord.len, 0x10);
	ASSERT_EQ(dataRecord.data[0], 0x21);
	ASSERT_EQ(dataRecord.data[dataRecord.len-1], 0x01);
}

}
