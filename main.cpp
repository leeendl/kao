#include "dpp/cluster.h" // @note dpp::cluster::

int main()
{
    dpp::cluster cluster("", dpp::i_all_intents);

    cluster.dpp::cluster::on_ready([](const dpp::ready_t &event)
    {
        puts("kao is ready!");
    });

    puts("kao has started up!");
    cluster.dpp::cluster::start(dpp::st_wait);

    return 0;
}