#include "Condition.h"
#include "Text.h"

namespace {

class NewCondition : public ConditionPolicy {
public:
    std::string code() const override { return "NEW"; }
    std::string title() const override { return "Новое"; }
    double valueFactor() const override { return 1.00; }
    bool availableForLoan() const override { return true; }
};

class GoodCondition : public ConditionPolicy {
public:
    std::string code() const override { return "GOOD"; }
    std::string title() const override { return "Хорошее"; }
    double valueFactor() const override { return 0.85; }
    bool availableForLoan() const override { return true; }
};

class WornCondition : public ConditionPolicy {
public:
    std::string code() const override { return "WORN"; }
    std::string title() const override { return "Потёртое"; }
    double valueFactor() const override { return 0.60; }
    bool availableForLoan() const override { return true; }
};

class BadCondition : public ConditionPolicy {
public:
    std::string code() const override { return "BAD"; }
    std::string title() const override { return "Ветхое"; }
    double valueFactor() const override { return 0.30; }
    bool availableForLoan() const override { return true; }
};

class LostCondition : public ConditionPolicy {
public:
    std::string code() const override { return "LOST"; }
    std::string title() const override { return "Утеряно"; }
    double valueFactor() const override { return 0.00; }
    bool availableForLoan() const override { return false; }
};

const NewCondition kNew;
const GoodCondition kGood;
const WornCondition kWorn;
const BadCondition kBad;
const LostCondition kLost;

}  // namespace

const std::vector<const ConditionPolicy*>& ConditionRegistry::all() {
    static const std::vector<const ConditionPolicy*> policies = {
        &kNew, &kGood, &kWorn, &kBad, &kLost
    };
    return policies;
}

const ConditionPolicy* ConditionRegistry::byCode(const std::string& code) {
    const std::string key = foldCase(trim(code));
    for (const auto* policy : all()) {
        if (foldCase(policy->code()) == key) return policy;
    }
    return nullptr;
}

std::string ConditionRegistry::defaultCode() {
    return "GOOD";
}

std::string conditionTitle(const std::string& code) {
    const ConditionPolicy* policy = ConditionRegistry::byCode(code);
    return policy ? policy->title() : "Неизвестно";
}

double conditionFactor(const std::string& code) {
    const ConditionPolicy* policy = ConditionRegistry::byCode(code);
    return policy ? policy->valueFactor() : 0.0;
}

bool conditionAvailable(const std::string& code) {
    const ConditionPolicy* policy = ConditionRegistry::byCode(code);
    return policy ? policy->availableForLoan() : false;
}
