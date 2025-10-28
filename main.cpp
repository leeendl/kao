#include "dpp/cluster.h" // @note dpp::cluster::
#include "dpp/once.h" // @note dpp::run_once

#include <format> // @note std::format()

int main()
{
    dpp::cluster cluster("", dpp::i_all_intents);

    cluster.on_log(dpp::utility::cout_logger());

    cluster.on_ready([&cluster](const dpp::ready_t &event)
    {
        puts("kao is ready!");
        if (dpp::run_once<struct register_bot_commands>()) 
        {
            cluster.global_command_create(dpp::slashcommand("ping", "ping pong!", cluster.me.id));
            cluster.global_command_create(
                dpp::slashcommand("purge", "purge x messages", cluster.me.id)
                    .set_default_permissions(dpp::p_manage_messages)
                    .add_option(
                        dpp::command_option(dpp::co_integer, "x", "x amount of messages to delete", true)
                            .set_max_value(100).set_min_value(1)
                    )
            );
        }
    });

    cluster.on_slashcommand([&cluster](const dpp::slashcommand_t &event) -> dpp::task<void> 
    {
        if (event.command.get_command_name() == "ping") 
        {
            co_await event.co_reply(std::format(":ping_pong: pong! {:.2f}ms", cluster.rest_ping * 1000));
        }
        else if (event.command.get_command_name() == "purge") 
        {
            int64_t x = std::get<int64_t>(event.get_parameter("x"));
            
            std::vector<dpp::snowflake> messages_id{};
            auto messages = co_await cluster.co_messages_get(event.command.channel_id, 0, event.command.id, 0, x);
            for (const auto &[id, mt] : std::get<dpp::message_map>(messages.value))
            {
                // @todo check if older than 14 days
                messages_id.emplace_back(id);
            }

            co_await cluster.co_message_delete_bulk(messages_id, event.command.channel_id);
        }

        co_return;
    });
    

    puts("kao has started up!");
    cluster.start(dpp::st_wait);

    return 0;
}