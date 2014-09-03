#include "broker/broker.hh"
#include "broker/endpoint.hh"
#include "broker/data/master.hh"
#include "broker/data/clone.hh"
#include "broker/data/frontend.hh"
#include "broker/data/response_queue.hh"
#include "testsuite.hh"
#include <iostream>
#include <map>
#include <vector>
#include <unistd.h>
#include <poll.h>

using namespace std;
using dataset = map<broker::data::key, broker::data::value>;

dataset get_contents(const broker::data::frontend& store)
	{
	dataset rval;

	for ( const auto& key : broker::data::keys(store) )
		{
		auto val = broker::data::lookup(store, key);
		if ( val ) rval.insert(make_pair(key, *val.get()));
		}

	return rval;
	}

bool compare_contents(const broker::data::frontend& store, const dataset& ds)
	{
	return get_contents(store) == ds;
	}

bool compare_contents(const broker::data::frontend& a,
                      const broker::data::frontend& b)
	{
	return get_contents(a) == get_contents(b);
	}

void wait_for(broker::data::frontend f, broker::data::key k, bool exists = true)
	{
	while ( broker::data::exists(f, k) != exists ) usleep(1000);
	}

int main(int argc, char** argv)
	{
	broker::init();
	broker::endpoint server("server");
	broker::data::master master(server, "mystore");

	dataset ds0 = { make_pair("1", "one"),
	                make_pair("2", "two"),
	                make_pair("3", "three") };
	for ( const auto& p : ds0 ) master.insert(p.first, p.second);

	BROKER_TEST(compare_contents(master, ds0));

	// TODO: better way to distribute ports among tests so they can go parallel
	if ( ! server.listen(9999, "127.0.0.1") )
		{
		cerr << server.last_error() << endl;
		return 1;
		}

	broker::endpoint client("client");
	broker::data::frontend frontend(client, "mystore");
	broker::data::clone clone(client, "mystore",
	                          std::chrono::duration<double>(0.25));

	client.peer("127.0.0.1", 9999).handshake();

	BROKER_TEST(compare_contents(frontend, ds0));
	BROKER_TEST(compare_contents(clone, ds0));

	master.insert("5", "five");
	BROKER_TEST(*broker::data::lookup(master, "5") == "five");
	BROKER_TEST(compare_contents(frontend, master));
	BROKER_TEST(compare_contents(clone, master));

	master.erase("5");
	BROKER_TEST(!broker::data::exists(master, "5"));
	BROKER_TEST(compare_contents(frontend, master));
	BROKER_TEST(compare_contents(clone, master));

	frontend.insert("5", "five");
	wait_for(master, "5");
	BROKER_TEST(compare_contents(frontend, master));
	BROKER_TEST(compare_contents(clone, master));

	frontend.erase("5");
	wait_for(master, "5", false);
	BROKER_TEST(compare_contents(frontend, master));
	BROKER_TEST(compare_contents(clone, master));

	clone.insert("5", "five");
	wait_for(master, "5");
	BROKER_TEST(compare_contents(frontend, master));
	BROKER_TEST(compare_contents(clone, master));

	clone.erase("5");
	wait_for(master, "5", false);
	BROKER_TEST(compare_contents(frontend, master));
	BROKER_TEST(compare_contents(clone, master));

	master.clear();
	BROKER_TEST(broker::data::size(master) == 0);
	BROKER_TEST(compare_contents(frontend, master));
	BROKER_TEST(compare_contents(clone, master));

	broker::done();
	return BROKER_TEST_RESULT();
	}
