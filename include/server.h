// vim: noet ts=4 sw=4
#pragma once

int static_handler(const http_request *request, http_response *response);
int index_handler(const http_request *request, http_response *response);
int datum_handler_save(const http_request *request, http_response *response);
int datum_handler_delete(const http_request *request, http_response *response);
int datum_handler(const http_request *request, http_response *response);
int data_handler(const http_request *request, http_response *response);
int data_handler_filter(const http_request *request, http_response *response);
int favicon_handler(const http_request *request, http_response *response);
