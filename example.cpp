#include <iostream>
#include "cow.h"

struct A
{
    int value = 0;

    A(int v) : value(v) {}
};

int main()
{
    cow::vector<std::shared_ptr<A>> v1;

    v1.push_back(std::make_shared<A>(1));  // v1 == { A(1) }
    v1.emplace_back(new A(2));  // v1 == { A(1), A(2) }

    for (auto v : v1)
        std::cout << v->value << '\n';

    cow::vector<std::shared_ptr<A>> v2(v1); // v2 == v1 == { A(1), A(2) }
    v2.push_front(std::make_shared<A>(3));  // now v2 == { A(3), A(1), A(2) }, but v2 is still { A(1), A(2) }

    // iterate by index using read-only copy
    auto readonly_copy = v2.read_only_copy();
    for (std::size_t i = 0; i < readonly_copy.size(); ++i)
        std::cout << readonly_copy[i]->value << '\n';

    // remove using predicate
    bool exists3 = v2.exists([](auto const& a) -> bool { return a->value == 3; });
    std::cout << "exists 3: " << exists3 << '\n';
    bool exists4 = v2.exists([](auto const& a) -> bool { return a->value == 4; });
    std::cout << "exists 4: " << exists4 << '\n';

    // remove using predicate
    std::size_t removed = v2.remove([](auto const& a) -> bool {
        return a->value == 3 || a->value == 2;
    });

    std::cout << "number of removed " << removed << '\n'; // now v2 == { A(1) }
    for (auto v : v2)
        std::cout << v->value << '\n';

    // we can remove or insert durin for loop, but it does not affect immediately, because it's a copy
    std::cout << "\n loop:\n"; // v1 == { A(1), A(2) }
    for (auto v : v1)
    {
        v1.remove([](auto const& a) -> bool { return a->value == 2; });
        v1.push_front(std::make_shared<A>(2));

        std::cout << v->value << '\n';
    }

    std::cout << "\n after loop:\n"; // v1 == { A(2), A(1) }
    for (auto v : v1)
        std::cout << v->value << '\n';

    auto a2 = v1.find_first([](auto const& a) -> bool { return a->value == 2; }, std::shared_ptr<A>());

    // direct acces should be done only under lock
    {
        std::lock_guard<std::mutex> locker(v1.lock());
        std::vector<std::shared_ptr<A>> & v = v1.data();
        v[0] = std::make_shared<A>(5);
    }

    return 0;
}

