#pragma once

#include <iostream>

#include <vulkan/vulkan.hpp>

#ifdef _WIN64
	#define NOMINMAX
	#include <windows.h>
#endif

namespace MilkShake
{
	namespace vulkan
	{
		static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
			VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
			VkDebugUtilsMessageTypeFlagsEXT messageType,
			const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
			void* pUserData)
		{
			bool isLogInfo = false;

			HANDLE hConsole = GetStdHandle(STD_ERROR_HANDLE);
			switch (messageSeverity)
			{
				case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
					#ifdef _WIN64
						SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
					#else
						// Linux: 
					#endif

					std::cerr << "[VERBOSE] ";
					isLogInfo = true;
					break;

				case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
					#ifdef _WIN64
						SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
					#else
						// Linux: 
					#endif

					std::cerr << "[INFO] ";
					isLogInfo = true;
					break;

				case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
					#ifdef _WIN64
						SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
					#else
						// Linux: 
					#endif

					std::cerr << "[WARNING] ";
					isLogInfo = true;
					break;

				case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
					#ifdef _WIN64
						SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_INTENSITY);
					#else
						// Linux: 
					#endif

					std::cerr << "[ERROR] ";
					isLogInfo = true;
					break;
			}

			if (isLogInfo)
			{
				std::cerr << pCallbackData->pMessage << "\n";
				#ifdef _WIN64
					SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
				#else
					// Linux: 
				#endif
			}

			return VK_FALSE;
		}
		static VkResult CreateDebugUtilsMessengerEXT(VkInstance m_Instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger)
		{
			PFN_vkCreateDebugUtilsMessengerEXT func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_Instance, "vkCreateDebugUtilsMessengerEXT");
			if (func != nullptr)
			{
				return func(m_Instance, pCreateInfo, pAllocator, pDebugMessenger);
			}
			else
			{
				return VK_ERROR_EXTENSION_NOT_PRESENT;
			}
		}
		static void DestroyDebugUtilsMessengerEXT(VkInstance m_Instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator)
		{
			// Pointer function of vkDestroyDebugUtilsMessengerEXT
			PFN_vkDestroyDebugUtilsMessengerEXT func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_Instance, "vkDestroyDebugUtilsMessengerEXT");
			if (func != nullptr)
				return func(m_Instance, debugMessenger, pAllocator);
		}
		static void SetupDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
		{
			createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
			// Which message severity will call pfnUserCallback (Verbose, Warning, Error)
			createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
				VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
				VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
			// Type of message callback get called
			createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
				VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
				VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
			createInfo.pfnUserCallback = DebugCallback;
			// Optional (Will automatically be null from createInfo{})
			createInfo.pUserData = nullptr;
		}
	}
}