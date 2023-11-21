// CuSimpleMatch V1 by chenzyadb.
// Based on C++14 STL (MSVC).

#ifndef _CU_SIMPLE_MATCH_H
#define _CU_SIMPLE_MATCH_H

#include <exception>
#include <string>
#include <vector>
#include <memory>

class MatchExcept : public std::exception
{
	public:
		MatchExcept(const std::string &message) : message_(message) { }

		const char* what() const noexcept override
		{
			return message_.c_str();
		}

	private:
		const std::string message_;
};

class CuSimpleMatch
{
	public:
		CuSimpleMatch();
		CuSimpleMatch(const std::string &rule);
		CuSimpleMatch(const CuSimpleMatch &other);
		~CuSimpleMatch();
		CuSimpleMatch &operator=(const CuSimpleMatch &other);
		bool operator==(const CuSimpleMatch &other) const;
		bool operator!=(const CuSimpleMatch &other) const;
		bool operator<(const CuSimpleMatch &other) const;
		bool operator>(const CuSimpleMatch &other) const;

		bool match(const std::string &text) const;
		std::string data() const;
		void setRule(const std::string &rule);
		void clear();

	private:
		std::string rule_;
		std::vector<std::string> front_;
		std::vector<std::string> middle_;
		std::vector<std::string> back_;
		std::vector<std::string> entire_;

		void UpdateRule_();
		std::vector<std::string> ParseKey_(const std::string &text);
};

#endif
