#include "../include/visky/context.h"
#include "transfers.h"
#include "vklite_utils.h"
#include <stdlib.h>



/*************************************************************************************************/
/*  Thread-safe FIFO queue                                                                       */
/*************************************************************************************************/

VklFifo vkl_fifo(int32_t capacity)
{
    log_trace("creating generic FIFO queue with a capacity of %d items", capacity);
    ASSERT(capacity >= 2);
    VklFifo fifo = {0};
    ASSERT(capacity <= VKL_MAX_FIFO_CAPACITY);
    fifo.capacity = capacity;

    if (pthread_mutex_init(&fifo.lock, NULL) != 0)
        log_error("mutex creation failed");
    if (pthread_cond_init(&fifo.cond, NULL) != 0)
        log_error("cond creation failed");

    return fifo;
}



void vkl_fifo_enqueue(VklFifo* fifo, void* item)
{
    ASSERT(fifo != NULL);
    pthread_mutex_lock(&fifo->lock);

    if ((fifo->head + 1) % fifo->capacity != fifo->tail)
    {
        // log_trace("enqueue item, head %d, tail %d", fifo->head, fifo->tail);
        fifo->items[fifo->head] = item;
        fifo->head++;
        if (fifo->head >= fifo->capacity)
            fifo->head -= fifo->capacity;
    }
    else
    {
        log_error("FIFO queue is full, reseting it");
        fifo->head = 0;
        fifo->tail = 0;
    }

    ASSERT(0 <= fifo->head && fifo->head < fifo->capacity);
    pthread_cond_signal(&fifo->cond);
    pthread_mutex_unlock(&fifo->lock);
}



void* vkl_fifo_dequeue(VklFifo* fifo, bool wait)
{
    ASSERT(fifo != NULL);
    pthread_mutex_lock(&fifo->lock);

    // Wait until the queue is not empty.
    if (wait)
    {
        log_trace("waiting for the queue to be non-empty");
        while (fifo->head == fifo->tail)
            pthread_cond_wait(&fifo->cond, &fifo->lock);
    }

    // Empty queue.
    if (fifo->head == fifo->tail)
    {
        // log_trace("FIFO queue was empty");
        // Don't forget to unlock the mutex before exiting this function.
        pthread_mutex_unlock(&fifo->lock);
        return NULL;
    }

    ASSERT(0 <= fifo->tail && fifo->tail < fifo->capacity);

    // log_trace("dequeue item, head %d, tail %d", fifo->head, fifo->tail);
    void* item = fifo->items[fifo->tail];

    fifo->tail++;
    if (fifo->tail >= fifo->capacity)
        fifo->tail -= fifo->capacity;

    ASSERT(0 <= fifo->tail && fifo->tail < fifo->capacity);
    pthread_mutex_unlock(&fifo->lock);

    return item;
}



int vkl_fifo_size(VklFifo* fifo)
{
    ASSERT(fifo != NULL);
    pthread_mutex_lock(&fifo->lock);
    log_debug("head %d tail %d", fifo->head, fifo->tail);
    int size = fifo->head - fifo->tail;
    if (size < 0)
        size += fifo->capacity;
    ASSERT(0 <= size && size <= fifo->capacity);
    pthread_mutex_unlock(&fifo->lock);
    return size;
}



void vkl_fifo_discard(VklFifo* fifo, int max_size)
{
    ASSERT(fifo != NULL);
    if (max_size == 0)
        return;
    pthread_mutex_lock(&fifo->lock);
    int size = fifo->head - fifo->tail;
    if (size < 0)
        size += fifo->capacity;
    ASSERT(0 <= size && size <= fifo->capacity);
    if (size > max_size)
    {
        log_trace(
            "discarding %d items in the FIFO queue which is getting overloaded", size - max_size);
        fifo->tail = fifo->head - max_size;
        if (fifo->tail < 0)
            fifo->tail += fifo->capacity;
    }
    pthread_mutex_unlock(&fifo->lock);
}



void vkl_fifo_reset(VklFifo* fifo)
{
    ASSERT(fifo != NULL);
    pthread_mutex_lock(&fifo->lock);
    fifo->head = 0;
    fifo->tail = 0;
    pthread_cond_signal(&fifo->cond);
    pthread_mutex_unlock(&fifo->lock);
}



void vkl_fifo_destroy(VklFifo* fifo)
{
    ASSERT(fifo != NULL);
    pthread_mutex_destroy(&fifo->lock);
    pthread_cond_destroy(&fifo->cond);
}



/*************************************************************************************************/
/*  Context                                                                                      */
/*************************************************************************************************/

static void _context_default_queues(VklGpu* gpu, VklWindow* window)
{
    vkl_gpu_queue(gpu, VKL_QUEUE_TRANSFER, VKL_DEFAULT_QUEUE_TRANSFER);
    vkl_gpu_queue(gpu, VKL_QUEUE_COMPUTE, VKL_DEFAULT_QUEUE_COMPUTE);
    vkl_gpu_queue(gpu, VKL_QUEUE_RENDER, VKL_DEFAULT_QUEUE_RENDER);
    if (window != NULL)
        vkl_gpu_queue(gpu, VKL_QUEUE_PRESENT, VKL_DEFAULT_QUEUE_PRESENT);
}



static void _context_default_buffers(VklContext* context)
{
    ASSERT(context != NULL);
    ASSERT(context->gpu != NULL);
    // Create a predetermined set of buffers.
    VklBuffer* buffer = NULL;
    for (uint32_t i = 0; i < VKL_BUFFER_TYPE_COUNT; i++)
    {
        buffer = vkl_container_alloc(&context->buffers);
        *buffer = vkl_buffer(context->gpu);
        ASSERT(buffer != NULL);

        // All buffers may be accessed from these queues.
        vkl_buffer_queue_access(buffer, VKL_DEFAULT_QUEUE_TRANSFER);
        vkl_buffer_queue_access(buffer, VKL_DEFAULT_QUEUE_COMPUTE);
        vkl_buffer_queue_access(buffer, VKL_DEFAULT_QUEUE_RENDER);
    }

    VkBufferUsageFlagBits transferable =
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    // Staging buffer
    {
        buffer = vkl_container_get(&context->buffers, VKL_BUFFER_TYPE_STAGING);
        ASSERT(buffer != NULL);
        vkl_buffer_type(buffer, VKL_BUFFER_TYPE_STAGING);
        vkl_buffer_size(buffer, VKL_BUFFER_TYPE_STAGING_SIZE);
        vkl_buffer_usage(buffer, transferable);
        vkl_buffer_memory(
            buffer, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        vkl_buffer_create(buffer);
        ASSERT(is_obj_created(&buffer->obj));

        // Permanently map the buffer.
        buffer->mmap = vkl_buffer_map(buffer, 0, VK_WHOLE_SIZE);
    }

    // Vertex buffer
    {
        buffer = vkl_container_get(&context->buffers, VKL_BUFFER_TYPE_VERTEX);
        ASSERT(buffer != NULL);
        vkl_buffer_type(buffer, VKL_BUFFER_TYPE_VERTEX);
        vkl_buffer_size(buffer, VKL_BUFFER_TYPE_VERTEX_SIZE);
        vkl_buffer_usage(
            buffer,
            transferable | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        vkl_buffer_memory(buffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        vkl_buffer_create(buffer);
        ASSERT(is_obj_created(&buffer->obj));
    }

    // Index buffer
    {
        buffer = vkl_container_get(&context->buffers, VKL_BUFFER_TYPE_INDEX);
        ASSERT(buffer != NULL);
        vkl_buffer_type(buffer, VKL_BUFFER_TYPE_INDEX);
        vkl_buffer_size(buffer, VKL_BUFFER_TYPE_INDEX_SIZE);
        vkl_buffer_usage(buffer, transferable | VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
        vkl_buffer_memory(buffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        vkl_buffer_create(buffer);
        ASSERT(is_obj_created(&buffer->obj));
    }

    // Storage buffer
    {
        buffer = vkl_container_get(&context->buffers, VKL_BUFFER_TYPE_STORAGE);
        ASSERT(buffer != NULL);
        vkl_buffer_type(buffer, VKL_BUFFER_TYPE_STORAGE);
        vkl_buffer_size(buffer, VKL_BUFFER_TYPE_STORAGE_SIZE);
        vkl_buffer_usage(buffer, transferable | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        vkl_buffer_memory(buffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        vkl_buffer_create(buffer);
        ASSERT(is_obj_created(&buffer->obj));
    }

    // Uniform buffer
    {
        buffer = vkl_container_get(&context->buffers, VKL_BUFFER_TYPE_UNIFORM);
        ASSERT(buffer != NULL);
        vkl_buffer_type(buffer, VKL_BUFFER_TYPE_UNIFORM);
        vkl_buffer_size(buffer, VKL_BUFFER_TYPE_UNIFORM_SIZE);
        vkl_buffer_usage(buffer, transferable | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
        vkl_buffer_memory(buffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        vkl_buffer_create(buffer);
        ASSERT(is_obj_created(&buffer->obj));
    }

    // Mappable uniform buffer
    {
        buffer = vkl_container_get(&context->buffers, VKL_BUFFER_TYPE_UNIFORM_MAPPABLE);
        ASSERT(buffer != NULL);
        vkl_buffer_type(buffer, VKL_BUFFER_TYPE_UNIFORM_MAPPABLE);
        vkl_buffer_size(buffer, VKL_BUFFER_TYPE_UNIFORM_SIZE);
        vkl_buffer_usage(buffer, transferable | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
        vkl_buffer_memory(
            buffer, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        vkl_buffer_create(buffer);
        ASSERT(is_obj_created(&buffer->obj));

        // Permanently map the buffer.
        buffer->mmap = vkl_buffer_map(buffer, 0, VK_WHOLE_SIZE);
    }
}



static void _destroy_resources(VklContext* context)
{
    ASSERT(context != NULL);

    log_trace("context destroy buffers");
    CONTAINER_DESTROY_ITEMS(VklBuffer, context->buffers, vkl_buffer_destroy)

    log_trace("context destroy sets of images");
    CONTAINER_DESTROY_ITEMS(VklImages, context->images, vkl_images_destroy)

    log_trace("context destroy samplers");
    CONTAINER_DESTROY_ITEMS(VklSampler, context->samplers, vkl_sampler_destroy)

    log_trace("context destroy textures");
    CONTAINER_DESTROY_ITEMS(VklTexture, context->textures, vkl_texture_destroy)

    log_trace("context destroy computes");
    CONTAINER_DESTROY_ITEMS(VklCompute, context->computes, vkl_compute_destroy)
}



VklContext* vkl_context(VklGpu* gpu, VklWindow* window)
{
    ASSERT(gpu != NULL);
    ASSERT(!is_obj_created(&gpu->obj));
    log_trace("creating context");

    VklContext* context = calloc(1, sizeof(VklContext));
    context->gpu = gpu;

    // Allocate memory for buffers, textures, and computes.
    context->buffers =
        vkl_container(VKL_CONTAINER_DEFAULT_COUNT, sizeof(VklBuffer), VKL_OBJECT_TYPE_BUFFER);
    context->images =
        vkl_container(VKL_CONTAINER_DEFAULT_COUNT, sizeof(VklImages), VKL_OBJECT_TYPE_IMAGES);
    context->samplers =
        vkl_container(VKL_CONTAINER_DEFAULT_COUNT, sizeof(VklSampler), VKL_OBJECT_TYPE_SAMPLER);
    context->textures =
        vkl_container(VKL_CONTAINER_DEFAULT_COUNT, sizeof(VklTexture), VKL_OBJECT_TYPE_TEXTURE);
    context->computes =
        vkl_container(VKL_CONTAINER_DEFAULT_COUNT, sizeof(VklCompute), VKL_OBJECT_TYPE_COMPUTE);

    // Specify the default queues.
    _context_default_queues(gpu, window);

    // Create the GPU after the default queues have been set.
    if (!is_obj_created(&gpu->obj))
    {
        VkSurfaceKHR surface = VK_NULL_HANDLE;
        if (window != NULL)
            surface = window->surface;
        vkl_gpu_create(gpu, surface);
    }

    // Create the default buffers.
    _context_default_buffers(context);

    context->transfer_cmd = vkl_commands(gpu, VKL_DEFAULT_QUEUE_TRANSFER, 1);
    context->fifo = vkl_fifo(VKL_MAX_FIFO_CAPACITY);

    gpu->context = context;
    obj_created(&context->obj);

    return context;
}



void vkl_context_reset(VklContext* context)
{
    ASSERT(context != NULL);
    log_trace("reset the context");
    _destroy_resources(context);
    _context_default_buffers(context);
}



void vkl_context_destroy(VklContext* context)
{
    if (context == NULL)
    {
        log_error("skip destruction of null context");
        return;
    }
    log_trace("destroying context");
    ASSERT(context != NULL);
    ASSERT(context->gpu != NULL);

    // Destroy the buffers, images, samplers, textures, computes.
    _destroy_resources(context);

    // Free the allocated memory.
    vkl_container_destroy(&context->buffers);
    vkl_container_destroy(&context->images);
    vkl_container_destroy(&context->samplers);
    vkl_container_destroy(&context->textures);
    vkl_container_destroy(&context->computes);

    vkl_fifo_destroy(&context->fifo);
}



/*************************************************************************************************/
/*  Buffer allocation                                                                            */
/*************************************************************************************************/

VklBufferRegions vkl_ctx_buffers(
    VklContext* context, VklBufferType buffer_type, uint32_t buffer_count, VkDeviceSize size)
{
    ASSERT(context != NULL);
    ASSERT(context->gpu != NULL);
    ASSERT(buffer_count > 0);
    ASSERT(size > 0);
    ASSERT(buffer_type < VKL_BUFFER_TYPE_COUNT);

    // Choose the first buffer with the requested type.
    VklBuffer* buffer = vkl_container_iter_init(&context->buffers);
    while (buffer != NULL)
    {
        if (is_obj_created(&buffer->obj) && buffer->type == buffer_type)
            break;
        buffer = vkl_container_iter(&context->buffers);
    }
    if (buffer == NULL)
    {
        log_error("could not find buffer with requested type %d", buffer_type);
        return (VklBufferRegions){0};
    }
    ASSERT(buffer != NULL);
    ASSERT(buffer->type == buffer_type);
    ASSERT(is_obj_created(&buffer->obj));

    VkDeviceSize alignment = 0;
    VkDeviceSize offset = buffer->allocated_size;
    bool needs_align =
        buffer_type == VKL_BUFFER_TYPE_UNIFORM || buffer_type == VKL_BUFFER_TYPE_UNIFORM_MAPPABLE;
    if (needs_align)
    {
        alignment = context->gpu->device_properties.limits.minUniformBufferOffsetAlignment;
        ASSERT(offset % alignment == 0); // offset should be already aligned
    }

    VklBufferRegions regions = vkl_buffer_regions(buffer, buffer_count, offset, size, alignment);
    VkDeviceSize alsize = regions.aligned_size;
    if (alsize == 0)
        alsize = size;
    ASSERT(alsize > 0);

    if (!is_obj_created(&buffer->obj))
    {
        log_error("invalid buffer %d", buffer_type);
        return regions;
    }

    // Check alignment for uniform buffers.
    if (needs_align)
    {
        ASSERT(alignment > 0);
        ASSERT(alsize % alignment == 0);
        for (uint32_t i = 0; i < buffer_count; i++)
            ASSERT(regions.offsets[i] % alignment == 0);
    }

    // Need to reallocate?
    if (offset + alsize * buffer_count > regions.buffer->size)
    {
        VkDeviceSize new_size = next_pow2(offset + alsize * buffer_count);
        log_info("reallocating buffer %d to %s", buffer_type, pretty_size(new_size));
        vkl_buffer_resize(regions.buffer, new_size, &context->transfer_cmd);
    }

    log_debug(
        "allocating %d buffers (type %d) with size %s (aligned size %s)", //
        buffer_count, buffer_type, pretty_size(size), pretty_size(alsize));
    ASSERT(offset + alsize * buffer_count <= regions.buffer->size);
    buffer->allocated_size += alsize * buffer_count;

    ASSERT(regions.offsets[buffer_count - 1] + alsize == buffer->allocated_size);
    return regions;
}



void vkl_ctx_buffers_resize(VklContext* context, VklBufferRegions* br, VkDeviceSize new_size)
{
    // NOTE: this function tries to resize a buffer region in-place, which only works if
    // it is the last allocated region in the buffer. Otherwise a brand new region is allocated,
    // which wastes space. TODO: smarter memory management, defragmentation etc.
    ASSERT(br->buffer != NULL);
    ASSERT(br->count > 0);
    if (br->count > 1)
    {
        log_error("vkl_buffer_regions_resize() currently only supports regions with buf count=1");
        return;
    }
    ASSERT(br->count == 1);

    // The region is the last allocated in the buffer, we can safely resize it.
    VkDeviceSize old_size = br->aligned_size > 0 ? br->aligned_size : br->size;
    ASSERT(old_size > 0);
    if (br->offsets[0] + old_size == br->buffer->allocated_size)
    {
        log_debug("resize the buffer region in-place");
        br->size = new_size;
        if (br->alignment > 0)
            br->aligned_size = aligned_size(new_size, br->alignment);
        br->buffer->allocated_size = br->offsets[0] + new_size;

        // Need to reallocate a new underlying buffer.
        if (br->offsets[0] + old_size > br->buffer->size)
        {
            VkDeviceSize bs = next_pow2(br->offsets[0] + old_size);
            log_info("reallocating buffer #%d to %s", br->buffer->type, pretty_size(bs));
            vkl_buffer_resize(br->buffer, bs, &context->transfer_cmd);
        }
    }

    // The region cannot be resized directly, need to make a new region allocation.
    else
    {
        log_debug("failed to resize the buffer region in-place, allocating a new region");
        *br = vkl_ctx_buffers(context, br->buffer->type, 1, new_size);
    }
}



/*************************************************************************************************/
/*  Compute                                                                                      */
/*************************************************************************************************/

VklCompute* vkl_ctx_compute(VklContext* context, const char* shader_path)
{
    ASSERT(context != NULL);
    ASSERT(shader_path != NULL);

    VklCompute* compute = vkl_container_alloc(&context->computes);
    *compute = vkl_compute(context->gpu, shader_path);
    return compute;
}



/*************************************************************************************************/
/*  Texture                                                                                      */
/*************************************************************************************************/

static VkImageType image_type_from_dims(uint32_t dims)
{
    switch (dims)
    {
    case 1:
        return VK_IMAGE_TYPE_1D;
        break;
    case 2:
        return VK_IMAGE_TYPE_2D;
        break;
    case 3:
        return VK_IMAGE_TYPE_3D;
        break;

    default:
        break;
    }
    log_error("invalid image dimensions %d", dims);
    return VK_IMAGE_TYPE_2D;
}



VklTexture* vkl_ctx_texture(VklContext* context, uint32_t dims, uvec3 size, VkFormat format)
{
    ASSERT(context != NULL);

    VklTexture* texture = vkl_container_alloc(&context->textures);
    VklImages* image = vkl_container_alloc(&context->images);
    VklSampler* sampler = vkl_container_alloc(&context->samplers);

    texture->context = context;
    *image = vkl_images(context->gpu, image_type_from_dims(dims), 1);
    *sampler = vkl_sampler(context->gpu);

    texture->image = image;
    texture->sampler = sampler;

    // Create the image.
    vkl_images_format(image, format);
    vkl_images_size(image, size[0], size[1], size[2]);
    vkl_images_tiling(image, VK_IMAGE_TILING_OPTIMAL);
    vkl_images_layout(image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    vkl_images_usage(
        image,                                                    //
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | //
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    vkl_images_memory(image, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vkl_images_queue_access(image, VKL_DEFAULT_QUEUE_TRANSFER);
    vkl_images_queue_access(image, VKL_DEFAULT_QUEUE_COMPUTE);
    vkl_images_queue_access(image, VKL_DEFAULT_QUEUE_RENDER);
    vkl_images_create(image);

    // Create the sampler.
    vkl_sampler_min_filter(sampler, VK_FILTER_NEAREST);
    vkl_sampler_mag_filter(sampler, VK_FILTER_NEAREST);
    vkl_sampler_address_mode(sampler, VKL_TEXTURE_AXIS_U, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
    vkl_sampler_address_mode(sampler, VKL_TEXTURE_AXIS_V, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
    vkl_sampler_address_mode(sampler, VKL_TEXTURE_AXIS_V, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
    vkl_sampler_create(sampler);

    obj_created(&texture->obj);

    // Immediately transition the image to its layout.
    {
        VklGpu* gpu = context->gpu;
        VklCommands* cmds = &context->transfer_cmd;

        vkl_cmd_reset(cmds, 0);
        vkl_cmd_begin(cmds, 0);

        VklBarrier barrier = vkl_barrier(gpu);
        vkl_barrier_stages(
            &barrier, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
        vkl_barrier_images(&barrier, image);
        vkl_barrier_images_layout(&barrier, VK_IMAGE_LAYOUT_UNDEFINED, image->layout);
        vkl_barrier_images_access(&barrier, 0, VK_ACCESS_TRANSFER_READ_BIT);
        vkl_cmd_barrier(cmds, 0, &barrier);

        vkl_cmd_end(cmds, 0);
        vkl_cmd_submit_sync(cmds, 0);
    }

    return texture;
}



void vkl_texture_resize(VklTexture* texture, uvec3 size)
{
    ASSERT(texture != NULL);
    ASSERT(texture->image != NULL);

    vkl_images_resize(texture->image, size[0], size[1], size[2]);
}



void vkl_texture_filter(VklTexture* texture, VklFilterType type, VkFilter filter)
{
    ASSERT(texture != NULL);
    ASSERT(texture->sampler != NULL);

    switch (type)
    {
    case VKL_FILTER_MIN:
        vkl_sampler_min_filter(texture->sampler, filter);
        break;
    case VKL_FILTER_MAX:
        vkl_sampler_mag_filter(texture->sampler, filter);
        break;
    default:
        log_error("invalid filter type %d", type);
        break;
    }
    vkl_sampler_destroy(texture->sampler);
    vkl_sampler_create(texture->sampler);
}



void vkl_texture_address_mode(
    VklTexture* texture, VklTextureAxis axis, VkSamplerAddressMode address_mode)
{
    ASSERT(texture != NULL);
    ASSERT(texture->sampler != NULL);

    vkl_sampler_address_mode(texture->sampler, axis, address_mode);

    vkl_sampler_destroy(texture->sampler);
    vkl_sampler_create(texture->sampler);
}



void vkl_texture_destroy(VklTexture* texture)
{
    ASSERT(texture != NULL);
    vkl_images_destroy(texture->image);
    vkl_sampler_destroy(texture->sampler);

    texture->image = NULL;
    texture->sampler = NULL;
    obj_destroyed(&texture->obj);
}



/*************************************************************************************************/
/*  Transfer queue                                                                               */
/*************************************************************************************************/

void vkl_transfer_mode(VklContext* context, VklTransferMode mode)
{
    ASSERT(context != NULL);
    context->transfer_mode = mode;
}



void vkl_transfer_loop(VklContext* context, bool wait)
{
    ASSERT(context != NULL);
    VklTransfer tr = {0};
    int res = 0;
    uint64_t counter = 0;
    while (res == 0)
    {
        // log_trace("transfer loop awaits for transfer task, iteration %d...", counter);
        // wait until a transfer task is available
        tr = fifo_dequeue(context, &context->fifo, wait);
        // process the dequeued task
        context->fifo.is_processing = true;
        res = process_transfer(context, tr);
        context->fifo.is_processing = false;
        counter++;
    }
    // log_trace("end transfer loop");
}



// Safe wait on a background thread until the transfer queue is empty. Periodically check the queue
// size.
void vkl_transfer_wait(VklContext* context, int poll_period)
{
    ASSERT(context != NULL);
    if (poll_period == 0)
        poll_period = VKL_TRANSFER_POLL_PERIOD;
    ASSERT(poll_period > 0);
    int size = 0;
    log_trace("waiting until the transfer queue is empty...");
    while (true)
    {
        size = vkl_fifo_size(&context->fifo);
        if (size == 0 && !context->fifo.is_processing)
            break;
        vkl_sleep(poll_period);
    }
    log_trace("the transfer queue is empty, stop waiting");
}



void vkl_transfer_reset(VklContext* context)
{
    ASSERT(context != NULL);
    vkl_fifo_reset(&context->fifo);
}



void vkl_transfer_stop(VklContext* context)
{
    ASSERT(context != NULL);
    // Enqueue a special object that causes the dequeue loop to end.
    VklTransfer tr = {0};
    tr.type = VKL_TRANSFER_NONE;
    fifo_enqueue(context, &context->fifo, tr);
}



/*************************************************************************************************/
/*  Data transfers                                                                               */
/*************************************************************************************************/

void vkl_upload_texture_region(
    VklContext* context, VklTexture* texture, uvec3 offset, uvec3 shape, VkDeviceSize size,
    void* data)
{
    ASSERT(texture != NULL);
    ASSERT(context != NULL);
    enqueue_texture_transfer(
        context, VKL_TRANSFER_TEXTURE_UPLOAD, texture, offset, shape, size, data);
}



void vkl_upload_texture(VklContext* context, VklTexture* texture, VkDeviceSize size, void* data)
{
    ASSERT(texture != NULL);
    ASSERT(context != NULL);

    uvec3 shape = {0};
    shape[0] = texture->image->width;
    shape[1] = texture->image->height;
    shape[2] = texture->image->depth;
    vkl_upload_texture_region(context, texture, (uvec3){0, 0, 0}, shape, size, data);
}



void vkl_download_texture_region(
    VklContext* context, VklTexture* texture, uvec3 offset, uvec3 shape, VkDeviceSize size,
    void* data)
{
    ASSERT(texture != NULL);
    ASSERT(context != NULL);
    enqueue_texture_transfer(
        context, VKL_TRANSFER_TEXTURE_DOWNLOAD, texture, offset, shape, size, data);
}



void vkl_download_texture(VklContext* context, VklTexture* texture, VkDeviceSize size, void* data)
{
    ASSERT(texture != NULL);
    ASSERT(context != NULL);
    ASSERT(size > 0);
    ASSERT(data != NULL);

    uvec3 shape = {0};
    shape[0] = texture->image->width;
    shape[1] = texture->image->height;
    shape[2] = texture->image->depth;
    vkl_download_texture_region(context, texture, (uvec3){0, 0, 0}, shape, size, data);
}



void vkl_upload_buffers(
    VklContext* context, VklBufferRegions regions, VkDeviceSize offset, VkDeviceSize size,
    void* data)
{
    ASSERT(context != NULL);
    ASSERT(size > 0);
    ASSERT(data != NULL);
    ASSERT(regions.count > 0);
    ASSERT(regions.buffer != VK_NULL_HANDLE);

    enqueue_regions_transfer(context, VKL_TRANSFER_BUFFER_UPLOAD, regions, offset, size, data);
}



void vkl_download_buffers(
    VklContext* context, VklBufferRegions regions, VkDeviceSize offset, VkDeviceSize size,
    void* data)
{
    ASSERT(context != NULL);
    ASSERT(size > 0);
    ASSERT(data != NULL);
    ASSERT(regions.count > 0);
    ASSERT(regions.buffer != VK_NULL_HANDLE);

    enqueue_regions_transfer(context, VKL_TRANSFER_BUFFER_DOWNLOAD, regions, offset, size, data);
}



void vkl_copy_buffers(
    VklContext* context,                           //
    VklBufferRegions src, VkDeviceSize src_offset, //
    VklBufferRegions dst, VkDeviceSize dst_offset, //
    VkDeviceSize size)
{
    ASSERT(context != NULL);
    ASSERT(src.buffer != NULL);
    ASSERT(dst.buffer != NULL);

    // Create the transfer object.
    VklTransfer tr = {0};
    tr.type = VKL_TRANSFER_BUFFER_COPY;
    tr.u.buf_copy.src = src;
    tr.u.buf_copy.dst = dst;
    tr.u.buf_copy.src_offset = src_offset;
    tr.u.buf_copy.dst_offset = dst_offset;
    tr.u.buf_copy.size = size;

    if (context->transfer_mode == VKL_TRANSFER_MODE_SYNC)
        process_transfer(context, tr);
    else
        fifo_enqueue(context, &context->fifo, tr);
}



void vkl_copy_textures(
    VklContext* context, VklTexture* src, uvec3 src_offset, VklTexture* dst, uvec3 dst_offset,
    uvec3 shape)
{
    ASSERT(context != NULL);
    ASSERT(src != NULL);
    ASSERT(dst != NULL);

    // Create the transfer object.
    VklTransfer tr = {0};
    tr.type = VKL_TRANSFER_TEXTURE_COPY;
    tr.u.tex_copy.src = src;
    tr.u.tex_copy.dst = dst;
    memcpy(tr.u.tex_copy.src_offset, src_offset, sizeof(uvec3));
    memcpy(tr.u.tex_copy.dst_offset, dst_offset, sizeof(uvec3));
    memcpy(tr.u.tex_copy.shape, shape, sizeof(uvec3));

    if (context->transfer_mode == VKL_TRANSFER_MODE_SYNC)
        process_transfer(context, tr);
    else
        fifo_enqueue(context, &context->fifo, tr);
}
