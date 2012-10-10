#include <pfq.hpp>
#include <future>
#include <system_error>

#include <sys/types.h>
#include <sys/wait.h>

#include "yats.hpp"

using namespace yats;
using namespace net;

Context(PFQ)
{
    Test(default_ctor_dtor)
    {
        pfq x;
        Assert(x.id(), is_equal_to(-1));
    }

    Test(move_ctor)
    {
        pfq x(64);
        pfq y(std::move(x));

        Assert(x.fd(), is_equal_to(-1));
        Assert(y.fd(), is_not_equal_to(-1));
    }


    Test(assign_move_oper)
    {
        pfq x(64);
        pfq y;
        y = std::move(x);
        
        Assert(x.fd(), is_equal_to(-1));
        Assert(y.fd(), is_not_equal_to(-1));
    }


    Test(swap)
    {
        pfq x(64);
        pfq y;
        x.swap(y);

        Assert(x.fd(), is_equal_to(-1));
        Assert(y.fd(), is_not_equal_to(-1));
    }


    Test(open_close)
    {
        pfq x;
        x.open(64);
        
        Assert(x.fd(), is_not_equal_to(-1));
        Assert(x.id(), is_not_equal_to(-1)); 
        AssertThrow( x.open(128) );

        x.close();
        Assert(x.fd(), is_equal_to(-1));
    }

    
    Test(enable_disable)
    {
        pfq x;
        
        AssertThrow(x.enable());
        AssertThrow(x.disable());

        x.open(64);
        
        x.enable();
        
        Assert(x.mem_addr());

        x.disable();

        Assert(x.mem_addr() == nullptr);
    }

    
    Test(is_enabled)
    {
        pfq x;
        Assert(x.is_enabled(), is_equal_to(false));
        x.open(64);
        Assert(x.is_enabled(), is_equal_to(false));
        x.enable();
        Assert(x.is_enabled(), is_equal_to(true));
    }


    Test(ifindex)
    {
        pfq x;
        AssertThrow(net::ifindex(1, "lo"));
        
        x.open(64);
        Assert( net::ifindex(x.fd(), "lo"), is_not_equal_to(-1));
    }


    Test(timestamp)
    {
        pfq x;
        AssertThrow(x.toggle_time_stamp(true));
        AssertThrow(x.time_stamp());

        x.open(64);
        x.toggle_time_stamp(true);

        Assert(x.time_stamp(), is_equal_to(true));
    }


    Test(caplen)
    {
        pfq x;
        AssertThrow(x.caplen(64));
        AssertThrow(x.caplen());

        x.open(64);
        x.caplen(128);

        Assert(x.caplen(), is_equal_to(128));
   
        x.enable();
        AssertThrow(x.caplen(64));
        x.disable();
        
        x.caplen(64);
        Assert(x.caplen(), is_equal_to(64));
    }
    
    
    Test(offset)
    {
        pfq x;
        AssertThrow(x.offset(14));
        AssertThrow(x.offset());

        x.open(64);
        x.offset(14);

        Assert(x.offset(), is_equal_to(14));
   
        x.enable();
        AssertThrow(x.offset(16));
        x.disable();
        
        x.offset(16);
        Assert(x.offset(), is_equal_to(16));
    }


    Test(slots)
    {
        pfq x;
        AssertThrow(x.slots(14));
        AssertThrow(x.slots());

        x.open(64);
        x.slots(1024);

        Assert(x.slots(), is_equal_to(1024));
   
        x.enable();
        AssertThrow(x.slots(4096));
        x.disable();
        
        x.slots(4096);
        Assert(x.slots(), is_equal_to(4096));
    }


    Test(slot_size)
    {
        pfq x;
        AssertThrow(x.slot_size());

        x.open(64);
        
        Assert(x.slot_size(), is_equal_to(96));
    }


    Test(bind_device)
    {
        pfq x;
        AssertThrow(x.bind("eth0"));

        x.open(group_policy::shared, 64);
    
        AssertThrow(x.bind("unknown"));
        x.bind("eth0");

        AssertThrow(x.bind_group(11, "eth0"));
    }
    

    Test(unbind_device)
    {
        pfq x;
        AssertThrow(x.unbind("eth0"));

        x.open(group_policy::shared, 64);
        
        AssertThrow(x.unbind("unknown"));
        x.unbind("eth0");
        
        AssertThrow(x.unbind_group(11, "eth0"));
    }


    Test(poll)
    {
        pfq x;
        AssertThrow(x.poll(10));

        x.open(64);
        x.poll(0);
    }


    Test(read)
    {
        pfq x;
        AssertThrow(x.read(10));

        x.open(64);
        AssertThrow(x.read(10));
        
        x.enable();
        Assert(x.read(10).empty());
    }


    Test(stats)
    {
        pfq x;
        AssertThrow(x.stats());

        x.open(64);

        auto s = x.stats();
        Assert(s.recv, is_equal_to(0));
        Assert(s.lost, is_equal_to(0));
        Assert(s.drop, is_equal_to(0));
    }

    Test(group_stats)
    {
        pfq x;

        x.open(64);

        AssertNoThrow(x.group_stats(11));

        x.join_group(11);

        auto s = x.group_stats(11);
        Assert(s.recv, is_equal_to(0));
        Assert(s.lost, is_equal_to(0));
        Assert(s.drop, is_equal_to(0));
    }


    Test(groups_mask)
    {
        pfq x;
        AssertThrow(x.groups_mask());

        x.open(64);

        Assert(x.groups_mask(), is_equal_to(0));

        auto v = x.groups();
        Assert(v.empty(), is_true());
    }

    Test(join_restricted)
    {
        pfq x(group_policy::restricted, 64);

        pfq y;

        y.open(group_policy::undefined, 64);

        Assert( y.join_group(x.group_id(), group_policy::restricted), is_equal_to(x.group_id()));
    }

    Test(join_deferred)
    {
        pfq x(group_policy::undefined, 64);

        x.join_group(42);
        x.join_group(42);
        
        auto task = std::async(std::launch::async, 
                    [&] {
                        pfq y(group_policy::undefined, 64);
                        Assert(y.join_group(42), is_equal_to(42));
                    });
        
        task.wait();
    }


    Test(join_restricted_thread)
    {
        pfq x(group_policy::restricted, 64);

        auto task = std::async(std::launch::async, 
                    [&] {
                        pfq y(group_policy::undefined, 64);
                        Assert(y.join_group(x.group_id(), group_policy::restricted), is_equal_to(x.group_id()));
                    });
        
        task.get(); // eventually rethrow the excpetion...
    }

    Test(join_restricted_process)
    {
        pfq x(group_policy::restricted, 64);
        pfq z(group_policy::shared, 64);

        auto p = fork();
        if (p == -1)
            throw std::system_error(errno, std::generic_category());

        if (p == 0) {
            pfq y(group_policy::undefined, 64);;
            
            Assert( y.join_group(z.group_id()), is_equal_to(z.group_id()));
            AssertThrow(y.join_group(x.group_id()));
        
            _Exit(1);
        }

        wait(nullptr);
    }

    Test(join_private_)
    {
        pfq x(64);

        pfq y(group_policy::undefined, 64);

        AssertThrow(y.join_group(x.group_id(), group_policy::restricted));
        AssertThrow(y.join_group(x.group_id(), group_policy::shared));
        AssertThrow(y.join_group(x.group_id(), group_policy::priv));
        AssertThrow(y.join_group(x.group_id(), group_policy::undefined));
    }

    Test(join_restricted_)
    {
        pfq x(group_policy::restricted, 64);

        pfq y(group_policy::undefined, 64);

        AssertNoThrow(y.join_group(x.group_id(), group_policy::restricted));
        
        AssertThrow(y.join_group(x.group_id(), group_policy::shared));
        AssertThrow(y.join_group(x.group_id(), group_policy::priv));
        AssertThrow(y.join_group(x.group_id(), group_policy::undefined));
    }

    Test(join_shared_)
    {
        pfq x(group_policy::shared, 64);

        pfq y(group_policy::undefined, 64);
        
        AssertNoThrow(y.join_group(x.group_id(), group_policy::shared));

        AssertThrow(y.join_group(x.group_id(), group_policy::restricted));
        AssertThrow(y.join_group(x.group_id(), group_policy::priv));
        AssertThrow(y.join_group(x.group_id(), group_policy::undefined));
    }


    Test(join_public)
    {
        pfq x;
        AssertThrow(x.join_group(12));

        x.open(64);
        int gid = x.join_group(0);
        Assert(gid, is_equal_to(0));

        gid = x.join_group(pfq::any_group);
        Assert(gid, is_equal_to(1));

        auto v = x.groups();

        Assert( v == (std::vector<int>{ 0, 1}) );

    }

    Test(leave_group)
    {
        pfq x;
        AssertThrow(x.leave_group(12));

        x.open(group_policy::shared, 64);
        int gid = x.join_group(42);
        Assert(gid, is_equal_to(42));

        x.leave_group(42);

        Assert(x.group_id(), is_equal_to(0));
        Assert(x.groups() == std::vector<int>{ 0 });
    }


    Test(gid)
    {
        pfq x;
        Assert(x.group_id(), is_equal_to(-1));
    }
}


int main(int argc, char *argv[])
{
    return yats::run(argc, argv);
}