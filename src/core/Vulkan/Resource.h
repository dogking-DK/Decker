

template <typename Handle>
class VulkanResource
{
public:
	using DeviceType = typename std::conditional<bindingType == vkb::BindingType::Cpp, vkb::core::HPPDevice, vkb::Device>::type;
	using ObjectType = typename std::conditional<bindingType == vkb::BindingType::Cpp, vk::ObjectType, VkObjectType>::type;

	VulkanResource(Handle handle = nullptr, DeviceType* device_ = nullptr);

	VulkanResource(const VulkanResource&) = delete;
	VulkanResource& operator=(const VulkanResource&) = delete;

	VulkanResource(VulkanResource&& other);
	VulkanResource& operator=(VulkanResource&& other);

	virtual ~VulkanResource() = default;

	const std::string& get_debug_name() const;
	DeviceType& get_device();
	DeviceType const& get_device() const;
	Handle& get_handle();
	const Handle& get_handle() const;
	uint64_t           get_handle_u64() const;
	ObjectType         get_object_type() const;
	bool               has_device() const;
	bool               has_handle() const;
	void               set_debug_name(const std::string& name);
	void               set_handle(Handle hdl);

private:
	// we always want to store a vk::Handle as a resource, so we have to figure out that type, depending on the BindingType!
	using ResourceType = typename vkb::VulkanTypeMapping<bindingType, Handle>::Type;

	std::string  debug_name;
	HPPDevice* device;
	ResourceType handle;
};