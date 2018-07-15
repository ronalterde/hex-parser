#include "hex.h"
#include <iostream>
#include <iomanip>

using namespace std;
using namespace hex;

static int feedIntoParser(HexParser& parser, const char* input) {
	const char* pos = input;
	size_t requestedCharCountPrev = 1;
	size_t requestedCharCount = 1;

	while(!parser.recordReady()) {
		if (parser.getError() != 0) {
			return parser.getError();
		}

		requestedCharCountPrev = requestedCharCount;
		requestedCharCount = parser.newData(pos, requestedCharCountPrev);
		pos += requestedCharCountPrev;
	}

	return 0;
}

static void printDataRecord(DataRecord& dataRecord) {
		cout << "Parsed Data record, payload { ";
		for (size_t i = 0; i < dataRecord.len; ++i)
			cout << std::setw(2) << std::setfill('0') << std::hex << int(dataRecord.data[i]);
		cout << " }" << endl;
}

int main(int argc, const char **argv) {
	if (argc != 2) {
		cerr << "Usage: " << argv[0] << " <HEX record>" << endl;
		return -1;
	}
	const char* input = argv[1];

	HexParser parser;

	int ret = feedIntoParser(parser, input);
	if (ret) {
		cerr << "Error while feeding into parser. Aborting." << endl;
		return ret;
	}

	if (parser.getRecordType() == RecordType::Data) {
		DataRecord dataRecord;
		parser.getData(dataRecord);
		printDataRecord(dataRecord);
	} else if (parser.getRecordType() == RecordType::EndOfFile) {
		cout << "Parsed EOF record." << endl;
	} else {
		cerr << "Unsupported record type: " << int(parser.getRecordType()) << endl;
		return -1;
	}

	return 0;
}
