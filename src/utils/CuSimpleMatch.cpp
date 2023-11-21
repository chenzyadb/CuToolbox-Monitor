#include "CuSimpleMatch.h"

CuSimpleMatch::CuSimpleMatch() : rule_(), front_(), middle_(), back_(), entire_() { }

CuSimpleMatch::CuSimpleMatch(const std::string &rule) : rule_(rule), front_(), middle_(), back_(), entire_()
{
	UpdateRule_();
}

CuSimpleMatch::CuSimpleMatch(const CuSimpleMatch &other) : rule_(other.data()), front_(), middle_(), back_(), entire_()
{
	UpdateRule_();
}

CuSimpleMatch::~CuSimpleMatch() { }

CuSimpleMatch &CuSimpleMatch::operator=(const CuSimpleMatch &other)
{
	if (rule_ != other.data()) {
		rule_ = other.data();
		UpdateRule_();
	}

	return *this;
}

bool CuSimpleMatch::operator==(const CuSimpleMatch &other) const
{
	return (rule_ == other.data());
}

bool CuSimpleMatch::operator!=(const CuSimpleMatch &other) const
{
	return (rule_ != other.data());
}

bool CuSimpleMatch::operator<(const CuSimpleMatch &other) const
{
	return (this < std::addressof(other));
}

bool CuSimpleMatch::operator>(const CuSimpleMatch &other) const
{
	return (this > std::addressof(other));
}

bool CuSimpleMatch::match(const std::string &text) const
{
	if (text.empty()) {
		return false;
	}
	for (const auto &key : front_) {
		if (text.find(key) == 0) {
			return true;
		}
	}
	for (const auto &key : middle_) {
		if (text.find(key) != std::string::npos) {
			return true;
		}
	}
	for (const auto &key : back_) {
		if (text.find(key) == (text.size() - key.size())) {
			return true;
		}
	}
	for (const auto &key : entire_) {
		if (text == key) {
			return true;
		}
	}
	return false;
}

std::string CuSimpleMatch::data() const
{
	return rule_;
}

void CuSimpleMatch::setRule(const std::string &rule)
{
	if (rule_ != rule) {
		rule_ = rule;
		UpdateRule_();
	}
}

void CuSimpleMatch::clear()
{
	rule_.clear();
	front_.clear();
	middle_.clear();
	back_.clear();
	entire_.clear();
}

void CuSimpleMatch::UpdateRule_()
{
	front_.clear();
	middle_.clear();
	back_.clear();
	entire_.clear();

	std::string key{};
	std::vector<std::string> keySet{};
	bool atKeySet = false, atFront = false, atBack = false;
	char frontChar = '\0', backChar = '\0';
	for (const auto &c : rule_) {
		if (!atKeySet && !atFront && !atBack) {
			atFront = true;
			frontChar = '\0';
			backChar = '\0';
		}
		switch (c) {
			case '^':
				if (atFront && frontChar == '\0') {
					frontChar = c;
				} else if (atBack && backChar == '\0') {
					backChar = c;
				} else {
					throw MatchExcept("Invalid Matching Rule.");
				}
				break;
			case '*':
				if (atFront && frontChar == '\0') {
					frontChar = c;
				} else if (atBack && backChar == '\0') {
					backChar = c;
				} else {
					throw MatchExcept("Invalid Matching Rule.");
				}
				break;
			case '(':
				if (atFront) {
					atKeySet = true;
					atFront = false;
				} else {
					throw MatchExcept("Invalid Matching Rule.");
				}
				break;
			case '|':
				if (atKeySet) {
					if (!key.empty()) {
						auto keys = ParseKey_(key);
						keySet.insert(keySet.end(), keys.begin(), keys.end());
						key.clear();
					}
				} else {
					throw MatchExcept("Invalid Matching Rule.");
				}
				break;
			case ')':
				if (atKeySet) {
					if (!key.empty()) {
						auto keys = ParseKey_(key);
						keySet.insert(keySet.end(), keys.begin(), keys.end());
						key.clear();
					}
					atKeySet = false;
					atBack = true;
				} else {
					throw MatchExcept("Invalid Matching Rule.");
				}
				break;
			case ';':
				if (atBack) {
					if (frontChar == '^' && backChar == '\0' || frontChar == '\0' && backChar == '*') {
						front_.insert(front_.end(), keySet.begin(), keySet.end());
					} else if (frontChar == '\0' && backChar == '^' || frontChar == '*' && backChar == '\0') {
						back_.insert(back_.end(), keySet.begin(), keySet.end());
					} else if (frontChar == '^' && backChar == '^' || frontChar == '\0' && backChar == '\0') {
						entire_.insert(entire_.end(), keySet.begin(), keySet.end());
					} else if (frontChar == '*' && backChar == '*') {
						middle_.insert(middle_.end(), keySet.begin(), keySet.end());
					}
					keySet.clear();
					atBack = false;
				} else {
					throw MatchExcept("Invalid Matching Rule.");
				}
				break;
			default:
				if (atKeySet) {
					key += c;
				}
				break;
		}
	}
}

std::vector<std::string> CuSimpleMatch::ParseKey_(const std::string &text)
{
	const auto getCharSet = [](const std::string &str) -> std::string {
		std::string charSet = "";
		if (str.size() == 2 && str[0] >= 'A' && str[0] <= 'Z' && str[1] >= 'a' && str[1] <= 'z') {
			charSet = str;
		} else if (str.size() == 3 && str[1] == '-' && 
			(str[0] >= '0' && str[0] <= '9' || str[0] >= 'A' && str[0] <= 'Z' || str[0] >= 'a' && str[0] <= 'z') &&
			(str[2] >= '0' && str[2] <= '9' || str[2] >= 'A' && str[2] <= 'Z' || str[2] >= 'a' && str[2] <= 'z')) 
		{
			for (char c = str[0]; c <= str[2]; c++) {
				charSet += c;
			}
		} else {
			throw MatchExcept("Invalid Matching Rule.");
		}
		return charSet;
	};

	std::vector<std::string> keys{};
	std::vector<std::string> charSets{};
	{
		std::string baseStr = "";
		bool inBracket = false;
		std::string charSetStr = "";
		for (const auto &c : text) {
			switch (c) {
				case '[':
					if (!inBracket) {
						inBracket = true;
						baseStr += '|';
					} else {
						throw MatchExcept("Invalid Matching Rule.");
					}
					break;
				case ']':
					if (inBracket) {
						inBracket = false;
						charSets.emplace_back(getCharSet(charSetStr));
						charSetStr.clear();
					} else {
						throw MatchExcept("Invalid Matching Rule.");
					}
					break;
				default:
					if (inBracket) {
						charSetStr += c;
					} else {
						baseStr += c;
					}
					break;
			}
		}
		keys.emplace_back(baseStr);
	}
	if (charSets.size() > 0) {
		size_t charSetIdx = 0;
		while (charSetIdx < charSets.size()) {
			auto prevKeys = keys;
			keys.clear();
			for (const auto &prevKey : prevKeys) {
				auto pos = prevKey.find("|");
				if (pos != std::string::npos) {
					for (const auto &c : charSets[charSetIdx]) {
						auto key = prevKey;
						key[pos] = c;
						keys.emplace_back(key);
					}
				}
			}
			charSetIdx++;
		}
	}

	return keys;
}
