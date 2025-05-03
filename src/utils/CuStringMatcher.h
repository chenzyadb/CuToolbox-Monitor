// CuStringMatcher by chenzyadb@github.com.
// Based on C++17 STL (MSVC).

#ifndef _CU_STRING_MATCHER_
#define _CU_STRING_MATCHER_

#include <exception>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <algorithm>
#include <cstring>

namespace CU
{
    class MatchExcept : public std::exception
    {
        public:
            MatchExcept(const std::string &message) : message_(message) { }

            const char* what() const noexcept override
            {
                return message_.data();
            }

        private:
            const std::string message_;
    };

    class StringMatcher
    {
        public:
            enum class MatchIndex : uint8_t {FRONT, MIDDLE, BACK, ENTIRE};

            StringMatcher() : matchRules_(), matchAll_(false) { }

            StringMatcher(const std::string &ruleText) : matchRules_(), matchAll_(false)
            {
                if (ruleText.size() == 0) {
                    return;
                }
                if (ruleText == "*") {
                    matchAll_ = true;
                    return;
                }
                enum class RuleIdx : uint8_t {RULE_FRONT, RULE_CONTENT, RULE_SET, RULE_BACK};
                auto idx = RuleIdx::RULE_FRONT;
                std::string ruleContent{};
                bool matchFront = true, matchBack = true;
                size_t pos = 0;
                while (pos < ruleText.size()) {
                    switch (ruleText[pos]) {
                        case '\\':
                            if ((pos + 1) == ruleText.size()) {
                                throw MatchExcept("Invalid matching rule");
                            }
                            if (idx == RuleIdx::RULE_FRONT) {
                                idx = RuleIdx::RULE_CONTENT;
                            }
                            pos++;
                            ruleContent += ruleText[pos];
                            break;
                        case '*':
                            if (idx == RuleIdx::RULE_FRONT && matchFront) {
                                matchFront = false;
                            } else if (idx == RuleIdx::RULE_CONTENT && matchBack) {
                                matchBack = false;
                            } else {
                                throw MatchExcept("Invalid matching rule");
                            }
                            break;
                        case '|':
                            if (idx == RuleIdx::RULE_CONTENT) {
                                idx = RuleIdx::RULE_BACK;
                            } else if (idx == RuleIdx::RULE_SET) {
                                ruleContent += ruleText[pos];
                            } else {
                                throw MatchExcept("Invalid matching rule");
                            }
                            break;
                        case '(':
                            if (idx == RuleIdx::RULE_FRONT) {
                                idx = RuleIdx::RULE_SET;
                            } else {
                                throw MatchExcept("Invalid matching rule");
                            }
                            break;
                        case ')':
                            if (idx == RuleIdx::RULE_SET) {
                                idx = RuleIdx::RULE_CONTENT;
                            } else {
                                throw MatchExcept("Invalid matching rule");
                            }
                            break;
                        default:
                            if (idx == RuleIdx::RULE_FRONT) {
                                idx = RuleIdx::RULE_CONTENT;
                            }
                            ruleContent += ruleText[pos];
                            break;
                    }
                    if (idx == RuleIdx::RULE_BACK || pos == (ruleText.size() - 1)) {
                        auto rules = parseRuleContent_(ruleContent);
                        if (matchFront && matchBack) {
                            auto &entire = matchRules_[MatchIndex::ENTIRE];
                            entire.insert(entire.end(), rules.begin(), rules.end());
                        } else if (matchFront && !matchBack) {
                            auto &front = matchRules_[MatchIndex::FRONT];
                            front.insert(front.end(), rules.begin(), rules.end());
                        } else if (!matchFront && matchBack) {
                            auto &back = matchRules_[MatchIndex::BACK];
                            back.insert(back.end(), rules.begin(), rules.end());
                        } else {
                            auto &middle = matchRules_[MatchIndex::MIDDLE];
                            middle.insert(middle.end(), rules.begin(), rules.end());
                        }
                        matchFront = true, matchBack = true;
                        ruleContent.clear();
                        idx = RuleIdx::RULE_FRONT;
                    }
                    pos++;
                }
            }

            StringMatcher(const StringMatcher &other) : 
                matchRules_(other._Get_MatchRules()),
                matchAll_(other._Is_MatchAll())
            { }

            StringMatcher(StringMatcher &&other) noexcept : 
                matchRules_(other._Get_MatchRules()),
                matchAll_(other._Is_MatchAll())
            { }

            ~StringMatcher() { }

            StringMatcher &operator=(const StringMatcher &other)
            {
                if (std::addressof(other) != this) {
                    matchRules_ = other._Get_MatchRules();
                    matchAll_ = other._Is_MatchAll();
                }
                return *this;
            }

            bool operator==(const StringMatcher &other) const
            {
                return (matchAll_ == other._Is_MatchAll() && matchRules_ == other._Get_MatchRules());
            }

            bool operator!=(const StringMatcher &other) const
            {
                return (matchAll_ != other._Is_MatchAll() || matchRules_ != other._Get_MatchRules());
            }

            bool match(const std::string &str, bool enableHotspotOpt = false) const
            {
                static const auto matchFront = [](const std::string &s, const std::string &p) -> bool {
                    size_t len = p.size();
                    if (s.size() < len) {
                        return false;
                    }
                    return (std::memcmp(s.data(), p.data(), len) == 0);
                };
                static const auto matchBack = [](const std::string &s, const std::string &p) -> bool {
                    size_t s_len = s.size(), p_len = p.size();
                    if (s_len < p_len) {
                        return false;
                    }
                    return (std::memcmp((s.data() + (s_len - p_len)), p.data(), p_len) == 0);
                };
                const auto countHotspot = [this, enableHotspotOpt](MatchIndex idx, const std::string &word) {
                    if (enableHotspotOpt) {
                        hotspotCounters_[idx][word]++;
                    }
                };

                if (matchAll_) {
                    return true;
                }
                if (str.empty()) {
                    return false;
                }
                if (matchRules_.count(MatchIndex::FRONT) == 1) {
                    const auto &front = matchRules_.at(MatchIndex::FRONT);
                    for (const auto &word : front) {
                        if (matchFront(str, word)) {
                            countHotspot(MatchIndex::FRONT, word);
                            return true;
                        }
                    }
                }
                if (matchRules_.count(MatchIndex::BACK) == 1) {
                    const auto &back = matchRules_.at(MatchIndex::BACK);
                    for (const auto &word : back) {
                        if (matchBack(str, word)) {
                            countHotspot(MatchIndex::BACK, word);
                            return true;
                        }
                    }
                }
                if (matchRules_.count(MatchIndex::ENTIRE) == 1) {
                    const auto &entire = matchRules_.at(MatchIndex::ENTIRE);
                    for (const auto &word : entire) {
                        if (str == word) {
                            countHotspot(MatchIndex::ENTIRE, word);
                            return true;
                        }
                    }
                }
                if (matchRules_.count(MatchIndex::MIDDLE) == 1) {
                    const auto &middle = matchRules_.at(MatchIndex::MIDDLE);
                    for (const auto &word : middle) {
                        if (str.find(word) != std::string::npos) {
                            countHotspot(MatchIndex::MIDDLE, word);
                            return true;
                        }
                    }
                }
                return false;
            }

            void hotspotOpt(bool ignoreUnusedWords = true)
            {
                static const auto indexs = {MatchIndex::FRONT, MatchIndex::MIDDLE, MatchIndex::BACK, MatchIndex::ENTIRE};
                for (auto index : indexs) {
                    if (hotspotCounters_.count(index) == 1) {
                        matchRules_[index] = 
                            hotspotOpt_Impl_(hotspotCounters_.at(index), matchRules_.at(index), ignoreUnusedWords);
                    }
                }
            }

            std::unordered_map<MatchIndex, std::vector<std::string>> _Get_MatchRules() const
            {
                return matchRules_;
            }

            bool _Is_MatchAll() const
            {
                return matchAll_;
            }

        private:
            typedef std::unordered_map<std::string, size_t> HotspotCounter; 

            std::unordered_map<MatchIndex, std::vector<std::string>> matchRules_;
            mutable std::unordered_map<MatchIndex, HotspotCounter> hotspotCounters_;
            bool matchAll_;

            static std::vector<std::string> parseRuleContent_(const std::string &content)
            {
                std::vector<std::string> rules{};
                size_t pos = 0, len = content.size();
                while (pos < len) {
                    auto next_pos = content.find('|', pos);
                    if (next_pos == std::string::npos) {
                        next_pos = len;
                    }
                    auto rule = content.substr(pos, next_pos - pos);
                    if (rule.find('[') != std::string::npos && rule.find(']') != std::string::npos) {
                        auto parsedRules = parseCharsets_(rule);
                        rules.insert(rules.end(), parsedRules.begin(), parsedRules.end());
                    } else {
                        rules.emplace_back(rule);
                    }
                    pos = next_pos + 1;
                }
                return rules;
            }

            static std::vector<std::string> parseCharsets_(const std::string &str)
            {
                std::vector<std::string> parsedRules{};
                parsedRules.emplace_back(str);
                size_t pos = 0;
                while (pos < parsedRules[0].size()) {
                    auto set_begin = parsedRules[0].find('[', pos);
                    if (set_begin == std::string::npos) {
                        break;
                    }
                    auto set_end = parsedRules[0].find(']', set_begin + 1);
                    if (set_end != std::string::npos) {
                        auto charSet = getCharset_(parsedRules[0].substr(set_begin + 1, set_end - set_begin - 1));
                        std::vector<std::string> expandedRules{};
                        for (const auto &rule : parsedRules) {
                            auto frontStr = rule.substr(0, set_begin);
                            auto backStr = rule.substr(set_end + 1);
                            for (const auto &ch : charSet) {
                                expandedRules.emplace_back(frontStr + ch + backStr);
                            }
                        }
                        parsedRules = expandedRules;
                    } else {
                        break;
                    }
                    pos = set_begin + 1;
                }
                return parsedRules;
            }

            static std::string getCharset_(const std::string &content)
            {
                static const auto isCharRange = [](char start_ch, char end_ch) -> bool {
                    if (start_ch >= '0' && start_ch <= '9' && end_ch >= '0' && end_ch <= '9') {
                        return true;
                    }
                    if (start_ch >= 'A' && start_ch <= 'Z' && end_ch >= 'A' && end_ch <= 'Z') {
                        return true;
                    }
                    if (start_ch >= 'a' && start_ch <= 'z' && end_ch >= 'a' && end_ch <= 'z') {
                        return true;
                    }
                    return false;
                };

                std::string charSet{};
                for (size_t pos = 0; pos < content.size(); pos++) {
                    if (content[pos] == '-' && pos > 0 && (pos + 1) < content.size()) {
                        if (isCharRange(content[pos - 1], content[pos + 1])) {
                            for (char ch = (content[pos - 1] + 1); ch < content[pos + 1]; ch++) {
                                charSet += ch;
                            }
                        } else {
                            charSet += content[pos];
                        }
                    } else {
                        charSet += content[pos];
                    }
                }
                return charSet;
            }

            std::vector<std::string> hotspotOpt_Impl_
                (const HotspotCounter &counter, const std::vector<std::string> &origMatchRule, bool ignoreUnusedWords)
            {
                class Hotspot_Item {
                    public:
                        Hotspot_Item() : count_(0), word_() { }

                        Hotspot_Item(size_t count, const std::string &word) : count_(count), word_(word) { }

                        Hotspot_Item(const Hotspot_Item &other) : count_(other.count()), word_(other.word()) { }

                        ~Hotspot_Item() { }

                        bool operator<(const Hotspot_Item &other) const
                        {
                            return (count_ < other.count());
                        }

                        bool operator>(const Hotspot_Item &other) const
                        {
                            return (count_ > other.count());
                        }

                        bool operator==(const Hotspot_Item &other) const
                        {
                            return (count_ == other.count() && word_ == other.word());
                        }

                        bool operator!=(const Hotspot_Item &other) const
                        {
                            return (count_ != other.count() || word_ != other.word());
                        }

                        size_t count() const 
                        {
                            return count_;
                        }

                        std::string word() const 
                        {
                            return word_;
                        }

                    private:
                        size_t count_;
                        std::string word_;
                };

                std::vector<Hotspot_Item> hotspots{};
                hotspots.reserve(origMatchRule.size());

                for (const auto &[word, count] : counter) {
                    hotspots.emplace_back(count, word);
                }
                if (!ignoreUnusedWords) {
                    for (const auto &word : origMatchRule) {
                        if (counter.count(word) == 0) {
                            hotspots.emplace_back(0, word);
                        }
                    }
                }

                std::sort(hotspots.begin(), hotspots.end(), std::greater<Hotspot_Item>());

                std::vector<std::string> optimizedMatchRule{};
                optimizedMatchRule.reserve(hotspots.size());

                for (const auto &item : hotspots) {
                    optimizedMatchRule.emplace_back(item.word());
                }

                return optimizedMatchRule;
            }
    };
}

#endif
