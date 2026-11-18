#pragma once

#include <string>
#include <vector>

// Паттерн "Стратегия": поведение экземпляра зависит от его состояния.
// Точка вариации — добавить новое состояние = новый класс + строка в ConditionRegistry::all().
class ConditionPolicy {
public:
    virtual ~ConditionPolicy() {}

    virtual std::string code() const = 0;
    virtual std::string title() const = 0;
    virtual double valueFactor() const = 0;
    virtual bool availableForLoan() const = 0;
};

class ConditionRegistry {
public:
    static const std::vector<const ConditionPolicy*>& all();
    static const ConditionPolicy* byCode(const std::string& code);
    static std::string defaultCode();
};

std::string conditionTitle(const std::string& code);
double conditionFactor(const std::string& code);
bool conditionAvailable(const std::string& code);
