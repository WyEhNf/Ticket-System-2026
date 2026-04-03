#pragma GCC optimize("Ofast")
#include "../include/system/System.hpp"
#include <iostream>
using namespace std;
void remove_data_file() {
  const char *files[] = {"ticket_system_ticket_tree.data",
                         "ticket_system_train_tree.data",
                         "ticket_system_user_tree.data",
                         "ticket_system_system.data",
                         "ticket_system.waiting",
                         "../build/ticket_system_ticket_tree.data",
                         "../build/ticket_system_train_tree.data",
                         "../build/ticket_system_user_tree.data",
                         "../build/ticket_system_system.data",
                         "../build/ticket_system.waiting"};
  for (const char *f : files) {
    std::filesystem::path p(f);
    if (std::filesystem::exists(p)) {
      std::filesystem::remove(p);
    }
  }
}
int main() {
  ios::sync_with_stdio(false);
  cin.tie(nullptr);

  sjtu::System sys("ticket_system");
  sys.run();
  return 0;
}