// vim: noet ts=4 sw=4
#pragma once

int static_handler(const m38_http_request *request, m38_http_response *response);
int index_handler(const m38_http_request *request, m38_http_response *response);
int datum_handler_save(const m38_http_request *request, m38_http_response *response);
int datum_handler_delete(const m38_http_request *request, m38_http_response *response);
int datum_handler(const m38_http_request *request, m38_http_response *response);
int data_handler(const m38_http_request *request, m38_http_response *response);
int data_handler_filter(const m38_http_request *request, m38_http_response *response);
int favicon_handler(const m38_http_request *request, m38_http_response *response);
