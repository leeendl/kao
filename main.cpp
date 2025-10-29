#include "dpp/cluster.h" // @note dpp::cluster::
#include "dpp/once.h" // @note dpp::run_once
#include "dpp/nlohmann/json.hpp"

#include <fstream> // @note std::ifstream()

class guild
{
public:
    uint64_t id{}; // @note session guild id (not saved in JSON)
    uint64_t autorole_id{};

    nlohmann::json to_json() const {
        return {
            {"autorole_id", this->autorole_id}
        };
    }
};
std::unordered_map<dpp::snowflake, guild> guilds{};

void kill_kao(int signal)
{
    for (const auto &guild : guilds)
    {
        std::ofstream o(std::format("guilds/{}.json", guild.first.str()));
        o << guild.second.to_json();
    }

    puts("killed kao");
    exit(0);
}

int main()
{
    std::signal(SIGINT, kill_kao);

    dpp::cluster cluster("", dpp::i_all_intents, 2);

    cluster.on_log(dpp::utility::cout_logger());

    cluster.on_ready([&cluster](const dpp::ready_t &event)
    {
        if (dpp::run_once<struct ready_once>()) 
        {
            puts("kao is ready!");
            cluster.global_command_create(dpp::slashcommand("ping", "ping pong!", cluster.me.id));
            cluster.global_command_create(
                dpp::slashcommand("purge", "purge x messages", cluster.me.id)
                    .set_default_permissions(dpp::p_manage_messages)
                    .add_option(
                        dpp::command_option(dpp::co_integer, "x", "x amount of messages to delete", true)
                            .set_max_value(100).set_min_value(1)
                    )
            );
            cluster.global_command_create(
                dpp::slashcommand("autorole", "add role to new members", cluster.me.id)
                    .set_default_permissions(dpp::p_manage_messages)
                    .add_option(
                        dpp::command_option(dpp::co_string, "role", "provide role ID or mention role", true)
                    )
            );
        }
    });

    cluster.on_guild_create([](const dpp::guild_create_t &event)
    {   
        ::guild guild{.id = event.created.id};

        std::string dir = std::format("guilds/{}.json", event.created.id.str());

        std::ifstream i(dir);
        if (!i.is_open())
        {
            std::ofstream o(dir);
            o << guild.to_json();
            i.open(dir);
        }

        nlohmann::json j = nlohmann::json::parse(i);
        guild = { 
            (j.contains("autorole_id")) ? j["autorole_id"].get<uint64_t>() : 0 
        };
        
        guilds.emplace(event.created.id, guild);
    });

    cluster.on_guild_member_add([&cluster](const dpp::guild_member_add_t &event) -> dpp::task<void> 
    {
        ::guild &guild = guilds[event.adding_guild.id];
        if (guild.autorole_id != 0) 
            co_await cluster.co_guild_member_add_role(guild.id, event.added.user_id, guild.autorole_id);

        co_return;
    });

    cluster.on_slashcommand([&cluster](const dpp::slashcommand_t &event) -> dpp::task<void> 
    {
        if (event.command.get_command_name() == "ping") 
        {
            co_await event.co_reply(std::format(":ping_pong: pong! {:.2f}ms on shard `{}`", cluster.rest_ping * 1000, event.from()->shard_id));
        }
        else if (event.command.get_command_name() == "autorole")
        {
            std::string x = std::get<std::string>(event.get_parameter("x"));
            std::erase_if(x, [](char c) { return !std::isdigit(c); });

            auto &autorole_id = guilds[event.command.guild_id].autorole_id;

            autorole_id = dpp::snowflake(x);
            co_await event.co_reply(std::format("adding <@&{}> to new members now!", autorole_id));
        }
        else if (event.command.get_command_name() == "purge") 
        {
            int64_t x = std::get<int64_t>(event.get_parameter("x"));
            
            std::vector<dpp::snowflake> messages_id{};
            auto messages = co_await cluster.co_messages_get(event.command.channel.id, 0, event.command.id, 0, x);
            for (const auto &[id, mt] : std::get<dpp::message_map>(messages.value))
            {
                // @todo check if older than 14 days
                messages_id.emplace_back(id);
            }

            co_await cluster.co_message_delete_bulk(messages_id, event.command.channel.id);
            co_await event.co_reply(std::format("deleted `{}` messages!", x));
        }

        co_return;
    });
    

    puts("kao has started up!");
    cluster.start(dpp::st_wait);

    return 0;
}