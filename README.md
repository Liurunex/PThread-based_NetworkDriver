# Background 
A piece of code is required which will function as a (de)multiplexor for a simple network device. Calls will be made to this software component to pass packets of data onto the network. Packets arriving from the network will be demultiplexed and passed to appropriate waiting callers. 
**The program moves packets of data between queues or buffers (amongst other things).**

# Specification
The program provides the central functionality of a network device driver. It will make calls onto an instance of the NetworkDevice ADT (provided by the test harness). Calls will be made to the program from the application level, as well.

The program is passing around pointers to PacketDescriptors. One of the components of the system is a PacketDescriptor constructor; this will be handed a region of memory and it divides that into pieces each the right size to hold a PacketDescriptor. The PacketDescriptors are then passed to another component, the FreePacketDescriptorStore. The FreePacketDescriptorStore is an unbounded container, which is used by the progrma and the test harness as a place from which to acquire PacketDescriptors. When they are no longer used, the PacketDescriptors must be returned to the FreePacketDescriptorStore.
