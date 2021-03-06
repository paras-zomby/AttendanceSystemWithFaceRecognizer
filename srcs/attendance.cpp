#include "attendance.h"
#include <fstream>
#include <utility>
using namespace std;

double Attendance::PersonWithData::getDiffHours(SYSTEMTIME a, SYSTEMTIME b)
{
    double diff = 0.0;
    if (a.wYear == b.wYear && a.wMonth == b.wMonth && a.wDay == b.wDay)
        diff = a.wHour - b.wHour + (a.wMinute - b.wMinute) / 60.0;
    return diff;
}

Attendance::TreeNode::TreeNode(const Person::PersonInfo &_person)
    : person(_person)
{
    left_child = nullptr;
    right_child = nullptr;
    flag = false;
}

Attendance::PersonWithData::PersonWithData(const Person::PersonInfo &info, std::vector<Person::CommuteData> data)
    : commute_data(std::move(data))
{
    name = info.name;
    id_number = info.id_number;
    identy = info.identy;
    face_id = info.face_id;
    state = info.state;
    SYSTEMTIME off_duty_time;
    for (const auto &cd: commute_data)
    {
        if (!cd.on_duty)
        {
            off_duty_time = cd.time;
        }
        else
        {
            auto diff = getDiffHours(off_duty_time, cd.time);
            if (diff > 0.01)
            {
                ++commute_times;
                commute_hours += diff;
            }
        }
    }
    avg_commute_hours = commute_times == 0 ? 0 : commute_hours / commute_times;
}

Person &Attendance::addPerson(Person::PersonInfo person)
{
    person.face_id = ++person_num;
    if (pTree == nullptr)
    {
        pTree = new TreeNode(person);
        return pTree->person;
    }
    else
    {
        node_ptr p = pTree;
        while (true)
        {
            if (person.face_id > p->person.info.face_id)
            {
                if (p->right_child != nullptr)
                    p = p->right_child;
                else
                {
                    p->right_child = new TreeNode(person);
                    p = p->right_child;
                    break;
                }
            }
            else
            {
                if (p->left_child != nullptr)
                    p = p->left_child;
                else
                {
                    p->left_child = new TreeNode(person);
                    p = p->left_child;
                    break;
                }
            }
        }
        return p->person;
    }
}

Person &Attendance::findPerson(int face_id)
{
    node_ptr p = pTree;
    while (p != nullptr)
    {
        if (face_id > p->person.info.face_id)
            p = p->right_child;
        else if (face_id < p->person.info.face_id)
            p = p->left_child;
        else
            return p->person;
    }
    return no_person;
}

void Attendance::destoryPersons()
{
    auto stack = new node_ptr[person_num + 5];
    int b = 0;
    node_ptr p = pTree;
    while (b > 0 || p != nullptr)
    {
        if (p != nullptr)
        {
            stack[b++] = p;
            p = p->left_child;
        }
        else
        {
            p = stack[--b];
            if (!p->flag)
            {
                stack[b++] = p;
                p->flag = true;
                p = p->right_child;
            }
            else
            {
                delete p;
                p = nullptr;
            }
        }
    }
    pTree = nullptr;
    person_num = 0;
}

vector<Attendance::PersonWithData> Attendance::derivePersonData()
{
    vector<PersonWithData> data;
    data.reserve(person_num);
    inorderVisit([&](node_ptr &p)
                 {
                     data.emplace_back(p->person.info, p->person.getCommuteData());
                 });
    return data;
}

bool Attendance::save(const string &path)
{
    ofstream f(path, ofstream::out | ofstream::binary);
    if (!f.is_open()) return false;
    inorderVisit([&](node_ptr &p)
                 {
                     p->person.info.save(f);
                     auto commute_data = p->person.getCommuteData();
                     auto size = commute_data.size();
                     f.write((char *) &size, sizeof(size));
                     f.write((char *) commute_data.data(), sizeof(Person::CommuteData) * size);
                 });
    f.close();
    return true;
}

bool Attendance::load(const string &path)
{
    ifstream f(path, ifstream::in | ifstream::binary);
    if (!f.is_open()) return false;
    f.peek();
    Person::PersonInfo info;
    Person::CommuteData tmp{};
    unsigned long long int size;
    person_num = 0;
    while (!f.eof())
    {
        info.load(f);
        auto &p = addPerson(info);
        ++person_num;
        f.read((char *) &size, sizeof(size));
        for (int i = 0; i < size; ++i)
        {
            f.read((char *) &tmp, sizeof(tmp));
            p.addCommuteData(tmp);
        }
        f.peek();
    }
    return true;
}

void Attendance::clearPersonData()
{
    inorderVisit([=](node_ptr &p)
                 {
                     p->person.clearCommuteData();
                 });
}

Attendance::~Attendance()
{
    destoryPersons();
}
